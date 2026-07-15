#!/usr/bin/env python3
# Full Silo System for Node-RED (admin API :3000):
# read 4 cables by-path -> parse -> live dashboard (text+gauge) -> store in InfluxDB 1.8
# + editable Silo Settings page (name/capacity/material, persisted to file) + offline watchdog.
import json, urllib.request, urllib.error

API="http://127.0.0.1:3000"; FLOW_ID="silosys_tab"
UITAB="silolive_uitab"; GRP="grp_live"; INFLUX="influx_silo_cfg"
UITSET="uitab_set"; GEDIT="grp_edit"; GVIEW="grp_view"
SETFILE="/root/.node-red/silo_settings.json"
CABLEFILE="/root/.node-red/silo_cablemap.json"
BYPATH="/dev/serial/by-path/platform-fd500000.pcie-pci-0000:01:00.0-usb-0:1.{n}:1.0-port0"
SILOS=[("silo1","Silo 1",1),("silo2","Silo 2",2),("silo3","Silo 3",3),("silo4","Silo 4",4)]

PARSE=r'''
const SID="__SID__";
const st=(flow.get("settings")||{})[SID]||{};
const NAME=st.name||"__DEF__";
const CAP=Number(st.capacity)||70000;
let raw=String(msg.payload==null?"":msg.payload);
const U=raw.toUpperCase();
let stable=/(^|[^A-Z])ST([^A-Z]|$)/.test(U)?true:(/(^|[^A-Z])US([^A-Z]|$)/.test(U)?false:null);
flow.set("seen_"+SID,Date.now());
if(/OVER|(^|[^A-Z])OL([^A-Z]|$)/.test(U)){msg.weight_kg=null;msg.status="overload";msg.stable=false;msg.silo_id=SID;msg.name=NAME;msg.text=NAME+": OVERLOAD";msg.payload=null;return msg;}
let m=null,re=/([+-]?)\s*([\d][\d .,]*\d|\d)\s*kg/gi,x;
while((x=re.exec(raw))!==null)m=x;
if(!m)return null;
let n=parseFloat((m[1]||"")+m[2].replace(/[ ,](?=\d{3}\b)/g,"").replace(",","."));
if(isNaN(n))return null;
let status=(n<-100||n>CAP)?"out_of_range":(stable===false?"settling":"ok");
flow.set("val_"+SID,n);
msg.weight_kg=n;msg.silo_id=SID;msg.name=NAME;msg.stable=stable;msg.status=status;
msg.text=NAME+": "+n.toLocaleString("en-ZA")+" kg"+(status==="out_of_range"?"  ⚠":(stable===false?"  (settling)":stable===true?"  ✓":""));
msg.payload=n;
return msg;
'''
TOFLUX=r'''
if(msg.weight_kg==null) return null;
msg.measurement="silo_weight";
msg.payload=[{weight_kg:msg.weight_kg, stable:(msg.stable===true), status:msg.status||"ok"},{silo_id:msg.silo_id}];
return msg;
'''
HB=r'''
const out=[];
for(const s of ["silo1","silo2","silo3","silo4"]){
  const v=flow.get("val_"+s); if(v==null) continue;
  out.push({measurement:"silo_weight", payload:[{weight_kg:v, stable:true, status:"heartbeat"},{silo_id:s}]});
}
return [out];
'''
def viewjs():
    return r'''
function viewStr(s){
  return ["silo1","silo2","silo3","silo4"].map(function(k){
    var o=s[k]||{}; return (o.name||k)+"  —  cap "+(o.capacity||70000)+" kg  —  "+(o.material||"(material not set)");
  }).join("\n");
}
'''
INIT=viewjs()+r'''
let s;
try{ s=JSON.parse(msg.payload||"{}"); }catch(e){ s=null; }
let writeBack=false;
if(!s || typeof s!=="object" || !Object.keys(s).length){
  s={silo1:{name:"Silo 1",capacity:70000,material:""},silo2:{name:"Silo 2",capacity:70000,material:""},
     silo3:{name:"Silo 3",capacity:70000,material:""},silo4:{name:"Silo 4",capacity:70000,material:""}};
  writeBack=true;
}
flow.set("settings",s);
return [ writeBack?{payload:JSON.stringify(s,null,2)}:null, {payload:viewStr(s)} ];
'''
COLLECT=r'''
flow.set("form_"+(msg.topic||"x"), msg.payload);
return null;
'''
APPLY=viewjs()+r'''
let s=flow.get("settings")||{};
let sid=flow.get("form_silo");
if(!sid){ return [null, {payload:"Pick a silo from the dropdown first."}]; }
s[sid]=s[sid]||{};
let nm=flow.get("form_name"), cap=flow.get("form_capacity"), mat=flow.get("form_material");
if(nm!==undefined && String(nm).trim()!=="") s[sid].name=String(nm).trim();
if(cap!==undefined && String(cap).trim()!=="") s[sid].capacity=Number(cap);
if(mat!==undefined && String(mat).trim()!=="") s[sid].material=String(mat).trim();
flow.set("settings",s);
flow.set("form_name","");flow.set("form_capacity","");flow.set("form_material","");
return [ {payload:JSON.stringify(s,null,2)}, {payload:viewStr(s)} ];
'''
WD=r'''
const now=Date.now(); const s=flow.get("settings")||{};
const outs=[null,null,null,null];
["silo1","silo2","silo3","silo4"].forEach(function(k,i){
  const seen=flow.get("seen_"+k); const nm=(s[k]&&s[k].name)||("Silo "+(i+1));
  if(!seen){ if(k==="silo3"){ outs[i]={payload: nm+": (spare — not connected)"}; } return; }
  if(now-seen>15000){ outs[i]={payload: nm+": ⚠ OFFLINE (last seen "+Math.round((now-seen)/1000)+"s ago)"}; }
});
return outs;
'''
SEED=viewjs()+r'''
var s={silo1:{name:"Silo 1",capacity:70000,material:""},silo2:{name:"Silo 2",capacity:70000,material:""},
       silo3:{name:"Silo 3",capacity:70000,material:""},silo4:{name:"Silo 4",capacity:70000,material:""}};
flow.set("settings",s);
return [ {payload:JSON.stringify(s,null,2)}, {payload:viewStr(s)} ];
'''
# ---- cable-swap safety check ----
CC_CMD=r'''for p in /dev/serial/by-path/*; do s=$(basename "$p" | sed -E "s/.*-usb-0:([0-9]+\.[0-9]+):.*/\1/"); t=$(readlink -f "$p"); id=NONE; for b in /dev/serial/by-id/*; do [ "$(readlink -f "$b")" = "$t" ] && id=$(basename "$b" | sed -E "s/.*Controller_([^-]*)-if.*/\1/"); done; echo "$s=$id"; done'''
CC_CHECK=r'''
var cur={};
String(msg.payload||"").trim().split("\n").forEach(function(l){var p=l.trim().split("=");if(p.length===2&&p[0])cur[p[0]]=p[1];});
if(!Object.keys(cur).length) return null;
flow.set("cablemap_current",cur);
var s=flow.get("settings")||{};
function nm(sock){var idx={"1.1":"silo1","1.2":"silo2","1.3":"silo3","1.4":"silo4"}[sock]; return (s[idx]&&s[idx].name)||idx||sock;}
var exp=flow.get("cablemap_expected");
if(!exp || !Object.keys(exp).length){
  flow.set("cablemap_expected",cur);
  return [ {payload:"Cable check: learned current layout ("+Object.keys(cur).length+" cables). Will alarm if a cable is moved."}, {payload:JSON.stringify(cur,null,2)} ];
}
var bad=[];
Object.keys(exp).forEach(function(sock){ var now=cur[sock]||"MISSING"; if(now!==exp[sock]) bad.push(nm(sock)+" (socket "+sock+"): has "+now+", expected "+exp[sock]); });
if(bad.length===0) return [ {payload:"✅ Cable check: all cables in their correct sockets"}, null ];
return [ {payload:"⚠ CABLE MOVED — "+bad.join("   |   ")+"   → put it back, or press 'Accept current layout' on Settings"}, null ];
'''
CC_ACCEPT=r'''
var cur=flow.get("cablemap_current");
if(!cur || !Object.keys(cur).length) return [null, {payload:"No cable reading yet — wait a few seconds, then try again."}];
flow.set("cablemap_expected",cur);
return [ {payload:JSON.stringify(cur,null,2)}, {payload:"✅ Accepted the current cable layout as correct."} ];
'''
CC_LOAD=r'''
try{ var e=JSON.parse(msg.payload||"{}"); if(e && Object.keys(e).length) flow.set("cablemap_expected",e); }catch(x){}
return null;
'''

def req(method,path,body=None):
    data=json.dumps(body).encode() if body is not None else None
    r=urllib.request.Request(API+path,data=data,method=method,headers={"Content-Type":"application/json"})
    try:
        with urllib.request.urlopen(r,timeout=30) as resp:
            raw=resp.read().decode(); return resp.status,(json.loads(raw) if raw.strip() else {})
    except urllib.error.HTTPError as e:
        return e.code, e.read().decode()[:400]

_,flows=req("GET","/flows")
if isinstance(flows,list):
    for nd in flows:
        if nd.get("type")=="tab" and nd.get("label") in ("Silo Live (test)","Silo System"):
            c,_=req("DELETE","/flow/"+nd["id"]); print("removed",nd.get("label"),"->",c)

configs=[
 {"id":UITAB,"type":"ui_tab","name":"Silos","icon":"dashboard","order":98,"disabled":False,"hidden":False},
 {"id":GRP,"type":"ui_group","name":"Live Weights","tab":UITAB,"order":1,"disp":True,"width":"12","collapse":False},
 {"id":UITSET,"type":"ui_tab","name":"Silo Settings","icon":"settings","order":99,"disabled":False,"hidden":False},
 {"id":GEDIT,"type":"ui_group","name":"Edit a silo","tab":UITSET,"order":1,"disp":True,"width":"6","collapse":False},
 {"id":GVIEW,"type":"ui_group","name":"Current settings","tab":UITSET,"order":2,"disp":True,"width":"6","collapse":False},
 {"id":INFLUX,"type":"influxdb","hostname":"127.0.0.1","port":"8086","protocol":"http","database":"silo",
  "name":"silo18","usetls":False,"tls":"","influxdbVersion":"1.x","url":"http://localhost:8086","rejectUnauthorized":True},
]
nodes=[
 {"id":"influx_out","type":"influxdb out","z":FLOW_ID,"influxdb":INFLUX,"name":"write silo","measurement":"silo_weight",
  "precision":"","retentionPolicy":"","database":"silo","precisionV18FluxV20":"ms","retentionPolicyV18Flux":"","org":"","bucket":"","x":1160,"y":300,"wires":[]},
 {"id":"hb_inj","type":"inject","z":FLOW_ID,"name":"heartbeat 5m","props":[{"p":"payload"}],"repeat":"300","crontab":"",
  "once":False,"onceDelay":0.1,"topic":"","payload":"","payloadType":"date","x":150,"y":600,"wires":[["hb_fn"]]},
 {"id":"hb_fn","type":"function","z":FLOW_ID,"name":"heartbeat writes","func":HB,"outputs":1,"noerr":0,"initialize":"","finalize":"","libs":[],"x":380,"y":600,"wires":[["influx_out"]]},
 # settings load
 {"id":"set_inj","type":"inject","z":FLOW_ID,"name":"load settings on start","props":[{"p":"payload"}],"repeat":"","crontab":"",
  "once":True,"onceDelay":"1.5","topic":"","payload":"","payloadType":"date","x":170,"y":660,"wires":[["set_read"]]},
 {"id":"set_read","type":"file in","z":FLOW_ID,"name":"read settings file","filename":SETFILE,"filenameType":"str",
  "format":"utf8","chunk":False,"sendError":False,"encoding":"none","allProps":False,"x":390,"y":660,"wires":[["set_init"]]},
 {"id":"set_init","type":"function","z":FLOW_ID,"name":"init settings","func":INIT,"outputs":2,"noerr":0,"initialize":"","finalize":"","libs":[],"x":600,"y":660,"wires":[["set_file"],["set_view"]]},
 {"id":"set_catch","type":"catch","z":FLOW_ID,"name":"file missing","scope":["set_read"],"uncaught":False,"x":390,"y":720,"wires":[["set_defaults"]]},
 {"id":"set_defaults","type":"function","z":FLOW_ID,"name":"seed defaults","func":SEED,"outputs":2,"noerr":0,"initialize":"","finalize":"","libs":[],"x":600,"y":720,"wires":[["set_file"],["set_view"]]},
 {"id":"set_file","type":"file","z":FLOW_ID,"name":"save settings file","filename":SETFILE,"filenameType":"str",
  "appendNewline":False,"createDir":True,"overwriteFile":"true","encoding":"utf8","x":840,"y":690,"wires":[[]]},
 {"id":"set_view","type":"ui_text","z":FLOW_ID,"group":GVIEW,"order":1,"width":"6","height":"6","name":"current",
  "label":"","format":"<pre style='white-space:pre-wrap'>{{msg.payload}}</pre>","layout":"col-left","className":"","x":840,"y":650,"wires":[]},
 # settings edit form
 {"id":"set_dd","type":"ui_dropdown","z":FLOW_ID,"name":"Silo","label":"Which silo","tooltip":"","place":"Select a silo","group":GEDIT,"order":1,"width":"6","height":"1",
  "passthru":True,"multiple":False,"options":[{"label":"Silo 1","value":"silo1","type":"str"},{"label":"Silo 2","value":"silo2","type":"str"},{"label":"Silo 3","value":"silo3","type":"str"},{"label":"Silo 4","value":"silo4","type":"str"}],
  "payload":"","topic":"silo","topicType":"str","className":"","x":200,"y":80,"wires":[["set_collect"]]},
 {"id":"set_in_name","type":"ui_text_input","z":FLOW_ID,"name":"name","label":"New name","tooltip":"","group":GEDIT,"order":2,"width":"6","height":"1",
  "passthru":True,"mode":"text","delay":"0","topic":"name","sendOnBlur":True,"className":"","topicType":"str","x":200,"y":120,"wires":[["set_collect"]]},
 {"id":"set_in_cap","type":"ui_text_input","z":FLOW_ID,"name":"capacity","label":"New capacity (kg)","tooltip":"","group":GEDIT,"order":3,"width":"6","height":"1",
  "passthru":True,"mode":"number","delay":"0","topic":"capacity","sendOnBlur":True,"className":"","topicType":"str","x":200,"y":160,"wires":[["set_collect"]]},
 {"id":"set_in_mat","type":"ui_text_input","z":FLOW_ID,"name":"material","label":"New material","tooltip":"","group":GEDIT,"order":4,"width":"6","height":"1",
  "passthru":True,"mode":"text","delay":"0","topic":"material","sendOnBlur":True,"className":"","topicType":"str","x":200,"y":200,"wires":[["set_collect"]]},
 {"id":"set_collect","type":"function","z":FLOW_ID,"name":"collect form","func":COLLECT,"outputs":1,"noerr":0,"initialize":"","finalize":"","libs":[],"x":430,"y":140,"wires":[[]]},
 {"id":"set_btn","type":"ui_button","z":FLOW_ID,"name":"Save","group":GEDIT,"order":5,"width":"6","height":"1","passthru":False,"label":"Save","tooltip":"","color":"","bgcolor":"","className":"","icon":"","payload":"save","payloadType":"str","topic":"save","topicType":"str","x":200,"y":240,"wires":[["set_apply"]]},
 {"id":"set_apply","type":"function","z":FLOW_ID,"name":"apply settings","func":APPLY,"outputs":2,"noerr":0,"initialize":"","finalize":"","libs":[],"x":430,"y":240,"wires":[["set_file"],["set_view"]]},
 # watchdog
 {"id":"wd_inj","type":"inject","z":FLOW_ID,"name":"check offline 10s","props":[{"p":"payload"}],"repeat":"10","crontab":"",
  "once":False,"onceDelay":8,"topic":"","payload":"","payloadType":"date","x":160,"y":420,"wires":[["wd_fn"]]},
 {"id":"wd_fn","type":"function","z":FLOW_ID,"name":"offline watchdog","func":WD,"outputs":4,"noerr":0,"initialize":"","finalize":"","libs":[],"x":380,"y":420,"wires":[["tx_silo1"],["tx_silo2"],["tx_silo3"],["tx_silo4"]]},
 # cable-swap safety check
 {"id":"cc_load_inj","type":"inject","z":FLOW_ID,"name":"load cablemap","props":[{"p":"payload"}],"repeat":"","crontab":"","once":True,"onceDelay":"2.5","topic":"","payload":"","payloadType":"date","x":160,"y":760,"wires":[["cc_load_read"]]},
 {"id":"cc_load_read","type":"file in","z":FLOW_ID,"name":"read cablemap","filename":CABLEFILE,"filenameType":"str","format":"utf8","chunk":False,"sendError":False,"encoding":"none","allProps":False,"x":370,"y":760,"wires":[["cc_load_fn"]]},
 {"id":"cc_load_fn","type":"function","z":FLOW_ID,"name":"apply cablemap","func":CC_LOAD,"outputs":1,"noerr":0,"initialize":"","finalize":"","libs":[],"x":590,"y":760,"wires":[[]]},
 {"id":"cc_load_catch","type":"catch","z":FLOW_ID,"name":"","scope":["cc_load_read"],"uncaught":False,"x":370,"y":800,"wires":[[]]},
 {"id":"cc_inj","type":"inject","z":FLOW_ID,"name":"cable check 30s","props":[{"p":"payload"}],"repeat":"30","crontab":"","once":True,"onceDelay":"6","topic":"","payload":"","payloadType":"date","x":160,"y":480,"wires":[["cc_exec"]]},
 {"id":"cc_exec","type":"exec","z":FLOW_ID,"command":CC_CMD,"addpay":"","append":"","useSpawn":"false","timer":"","winHide":False,"oldrc":False,"name":"read cable ids","x":370,"y":480,"wires":[["cc_check"],[],[]]},
 {"id":"cc_check","type":"function","z":FLOW_ID,"name":"cable check","func":CC_CHECK,"outputs":2,"noerr":0,"initialize":"","finalize":"","libs":[],"x":580,"y":480,"wires":[["cc_text"],["cc_file"]]},
 {"id":"cc_text","type":"ui_text","z":FLOW_ID,"group":GRP,"order":0,"width":"12","height":"1","name":"cable check","label":"","format":"{{msg.payload}}","layout":"col-center","className":"","x":780,"y":480,"wires":[]},
 {"id":"cc_file","type":"file","z":FLOW_ID,"name":"save cablemap","filename":CABLEFILE,"filenameType":"str","appendNewline":False,"createDir":True,"overwriteFile":"true","encoding":"utf8","x":780,"y":520,"wires":[[]]},
 {"id":"cc_btn","type":"ui_button","z":FLOW_ID,"name":"Accept cables","group":GEDIT,"order":6,"width":"6","height":"1","passthru":False,"label":"Accept current cable layout","tooltip":"","color":"","bgcolor":"","className":"","icon":"","payload":"accept","payloadType":"str","topic":"accept","topicType":"str","x":200,"y":300,"wires":[["cc_accept"]]},
 {"id":"cc_accept","type":"function","z":FLOW_ID,"name":"accept cables","func":CC_ACCEPT,"outputs":2,"noerr":0,"initialize":"","finalize":"","libs":[],"x":430,"y":300,"wires":[["cc_file"],["cc_text"]]},
]
y=80
for sid,defn,n in SILOS:
    cfg="sp_"+sid; sin="in_"+sid; par="par_"+sid; txf="txf_"+sid; tx="tx_"+sid
    gg="gg_"+sid; rb="rb_"+sid; fx="fx_"+sid
    configs.append({"id":cfg,"type":"serial-port","name":sid,"serialport":BYPATH.format(n=n),"serialbaud":"9600","databits":"8",
      "parity":"none","stopbits":"1","waitfor":"","dtr":"none","rts":"none","cts":"none","dsr":"none","newline":"\\r","bin":"false","out":"char","addchar":"","responsetimeout":"10000"})
    nodes += [
      {"id":sin,"type":"serial in","z":FLOW_ID,"name":defn+" in","serial":cfg,"x":700,"y":y,"wires":[[par]]},
      {"id":par,"type":"function","z":FLOW_ID,"name":"parse "+sid,"func":PARSE.replace("__SID__",sid).replace("__DEF__",defn),
       "outputs":1,"noerr":0,"initialize":"","finalize":"","libs":[],"x":880,"y":y,"wires":[[txf,gg,rb]]},
      {"id":txf,"type":"function","z":FLOW_ID,"name":"txt "+sid,"func":"msg.payload=msg.text;return msg;","outputs":1,"noerr":0,"initialize":"","finalize":"","libs":[],"x":1060,"y":y-24,"wires":[[tx]]},
      {"id":tx,"type":"ui_text","z":FLOW_ID,"group":GRP,"order":n,"width":"12","height":"1","name":defn,"label":"","format":"{{msg.payload}}","layout":"col-center","className":"","x":1220,"y":y-24,"wires":[]},
      {"id":gg,"type":"ui_gauge","z":FLOW_ID,"name":defn,"group":GRP,"order":n+10,"width":"6","height":"4","gtype":"gage","title":defn,"label":"kg","format":"{{value}}","min":0,"max":"70000","colors":["#00b500","#e6e600","#ca3838"],"seg1":"","seg2":"","diff":False,"className":"","x":1060,"y":y+20,"wires":[]},
      {"id":rb,"type":"rbe","z":FLOW_ID,"name":"deadband 20","func":"deadband","gap":"20","start":"","inout":"out","septopics":True,"property":"payload","topi":"topic","x":1060,"y":y+60,"wires":[[fx]]},
      {"id":fx,"type":"function","z":FLOW_ID,"name":"toinflux "+sid,"func":TOFLUX,"outputs":1,"noerr":0,"initialize":"","finalize":"","libs":[],"x":1240,"y":y+60,"wires":[["influx_out"]]},
    ]
    y+=130

flow={"id":FLOW_ID,"label":"Silo System","disabled":False,"info":"","nodes":nodes,"configs":configs}
code,resp=req("POST","/flow",flow)
print("deploy POST /flow ->",code, resp if isinstance(resp,str) else resp.get("id",""))
