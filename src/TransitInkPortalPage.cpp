#include "TransitInkPortalPage.h"
#include "ProductConfig.h"

const char kTransitInkPortalHtml[] PROGMEM = R"HTML(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>)HTML" FIRMWARE_PRODUCT_NAME R"HTML( settings</title>
<style>
:root{color-scheme:light dark;--bg:#f4f6f9;--surface:#fff;--surface-2:#edf1f7;--text:#142033;--muted:#526176;--border:#c7d0dc;--accent:#1558d6;--accent-hover:#1047ad;--accent-text:#fff;--danger:#a31d2d;--focus:#0a66ff;--radius:12px;--shadow:0 8px 24px rgba(20,32,51,.09)}
*{box-sizing:border-box}html{background:var(--bg);color:var(--text);font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif;line-height:1.45}
body{margin:0;min-width:280px;padding-bottom:94px}button,input,select{font:inherit;color:inherit}button{cursor:pointer}button:disabled{cursor:not-allowed;opacity:.46}
button,input,select{border-radius:var(--radius)}button:focus-visible,input:focus-visible,select:focus-visible{outline:3px solid var(--focus);outline-offset:2px}
.shell{width:min(860px,100%);margin:0 auto;padding:24px 18px 34px}.masthead{display:flex;justify-content:space-between;align-items:flex-end;gap:20px;margin-bottom:20px}
.brand{font-size:28px;line-height:1.1;margin:0 0 5px;letter-spacing:-.02em}.context{margin:0;color:var(--muted)}.top-facts{margin:0;text-align:right;color:var(--muted);font-size:14px}
.tabs{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:6px;padding:6px;background:var(--surface-2);border:1px solid var(--border);border-radius:var(--radius);margin-bottom:18px}
.tab{min-height:44px;padding:8px 10px;border:0;background:transparent;color:var(--muted);font-weight:650}.tab[aria-selected="true"]{background:var(--surface);color:var(--accent);box-shadow:var(--shadow)}
.panel[hidden]{display:none}.panel{display:grid;gap:14px}.section{background:var(--surface);border:1px solid var(--border);border-radius:var(--radius);padding:20px}
h2{font-size:20px;margin:0 0 5px}.intro,.helper{color:var(--muted);margin:0}.helper{font-size:13px}
.field-grid{display:grid;grid-template-columns:1fr 1fr;gap:14px;margin-top:16px}.field{display:grid;gap:6px}.field.full{grid-column:1/-1}
label{font-size:14px;font-weight:650}input,select{width:100%;min-height:44px;padding:10px 12px;border:1px solid var(--border);background:var(--surface)}
.readonly-grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:1px;background:var(--border);border:1px solid var(--border);border-radius:var(--radius);overflow:hidden;margin-top:15px}
.read-row{background:var(--surface-2);padding:12px 14px;display:grid;gap:3px}.read-row span{color:var(--muted);font-size:13px}
.secondary{min-height:42px;padding:9px 14px;border:1px solid var(--border);background:var(--surface);color:var(--accent);font-weight:650}
.wifi-actions{display:flex;align-items:center;gap:10px;margin-top:14px}.wifi-actions .helper{flex:1}
.widget-list{display:grid;gap:10px}.widget-card{background:var(--surface);border:1px solid var(--border);border-radius:var(--radius);overflow:hidden}.widget-card[data-expanded="true"]{border-color:var(--accent);box-shadow:var(--shadow)}
.widget-head{display:grid;grid-template-columns:minmax(0,1fr) auto;gap:12px;padding:14px;align-items:center}.widget-toggle{border:0;background:transparent;padding:0;text-align:left;min-width:0}
.widget-title{display:block;font-weight:750}.widget-summary{display:block;color:var(--muted);font-size:13px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;margin-top:2px}
.order-actions{display:flex;gap:6px}.order-actions button{min-height:36px;padding:7px 9px}
.widget-body{border-top:1px solid var(--border);padding:16px;background:var(--surface-2)}.widget-controls{display:grid;grid-template-columns:1fr 1fr;gap:12px}
.widget-preview{margin:14px 0 0;padding:11px 12px;background:var(--surface);border-radius:var(--radius);color:var(--muted);font-size:14px}
.widget-error{min-height:20px;margin:8px 0 0;color:var(--danger);font-size:13px;font-weight:650}
.toggle-field{display:flex;align-items:center;gap:10px;min-height:44px}.toggle-field input{width:20px;min-height:20px;margin:0}
.action-bar{position:fixed;z-index:20;left:0;right:0;bottom:0;background:var(--surface);border-top:1px solid var(--border);box-shadow:0 -7px 22px rgba(20,32,51,.09)}
.action-inner{width:min(860px,100%);margin:0 auto;padding:12px 18px;display:flex;align-items:center;justify-content:space-between;gap:18px}
.status{margin:0;color:var(--muted);font-size:14px;white-space:pre-wrap}
.primary{min-height:46px;padding:11px 18px;border:1px solid var(--accent);background:var(--accent);color:var(--accent-text);font-weight:750}
@media(max-width:640px){body{padding-bottom:122px}.shell{padding:16px 12px 28px}.masthead{display:block}.top-facts{text-align:left;margin-top:9px}.field-grid,.widget-controls,.readonly-grid{grid-template-columns:1fr}.widget-head{grid-template-columns:1fr}.action-inner{display:grid;gap:8px}.primary{width:100%}}
@media(prefers-color-scheme:dark){:root{--bg:#0f1724;--surface:#182233;--surface-2:#202d40;--text:#f2f6fc;--muted:#afbed2;--border:#425168;--accent:#6ea2ff;--accent-hover:#8cb5ff;--accent-text:#071426;--danger:#ff8c9a;--focus:#8cb5ff;--shadow:0 8px 24px rgba(0,0,0,.24)}}
</style>
</head>
<body>
<div class="shell">
<header class="masthead"><div><p class="brand">)HTML" FIRMWARE_PRODUCT_NAME R"HTML(</p><p class="context">TTC surface arrivals for your e-ink dashboard</p></div><p class="top-facts" id="top_facts">Loading…</p></header>
<div class="tabs" role="tablist">
<button class="tab" id="tab_wifi" type="button" role="tab" aria-selected="true" aria-controls="panel_wifi" onclick="selectTab('wifi')">Wi-Fi</button>
<button class="tab" id="tab_widgets" type="button" role="tab" aria-selected="false" aria-controls="panel_widgets" onclick="selectTab('widgets')">Widgets</button>
<button class="tab" id="tab_power" type="button" role="tab" aria-selected="false" aria-controls="panel_power" onclick="selectTab('power')">Power</button>
</div>
<form id="settings_form" onsubmit="saveConfig(event)">
<section class="panel" id="panel_wifi" role="tabpanel">
<div class="section"><h2>Wi-Fi</h2><p class="intro">Connect the device so it can download live TTC trip updates.</p>
<div class="readonly-grid"><div class="read-row"><span>Current network</span><strong id="current_ssid">—</strong></div><div class="read-row"><span>Password</span><strong id="password_status">—</strong></div></div>
<div class="field-grid"><div class="field full"><label for="wifi_ssid">Network name</label><input id="wifi_ssid" name="wifi_ssid" maxlength="32" autocomplete="off"></div>
<div class="field full"><label for="wifi_password">Password</label><input id="wifi_password" name="wifi_password" type="password" maxlength="64" autocomplete="new-password" placeholder="Leave blank to keep the saved password"></div></div>
<div class="wifi-actions"><button class="secondary" type="button" onclick="scanWifi()">Scan networks</button><p class="helper" id="wifi_scan_status"></p></div>
</div></section>
<section class="panel" id="panel_widgets" role="tabpanel" hidden>
<div class="section"><h2>Widgets</h2><p class="intro">Configure up to four arrival slots. Each slot can show one TTC stop.</p><div class="widget-list" id="widget_cards"></div></div>
</section>
<section class="panel" id="panel_power" role="tabpanel" hidden>
<div class="section"><h2>Power</h2><p class="intro">Optional sleep and daily wake window.</p>
<div class="field-grid">
<div class="field full"><label class="toggle-field"><input id="sleep_enabled" type="checkbox" onchange="syncPowerFields()"> Enable sleep</label></div>
<div class="field"><label for="wake_duration_minutes">Wake duration (minutes)</label><input id="wake_duration_minutes" type="number" min="1" max="60" value="5"></div>
<div class="field"><label for="sleep_maintenance_hours">Maintenance wake (hours)</label><input id="sleep_maintenance_hours" type="number" min="0" max="24" value="12"></div>
<div class="field full"><label class="toggle-field"><input id="scheduled_wake_enabled" type="checkbox" onchange="syncPowerFields()"> Daily wake window</label></div>
<div class="field"><label for="scheduled_wake_start">Window start</label><input id="scheduled_wake_start" type="time" value="08:00"></div>
<div class="field"><label for="scheduled_wake_end">Window end</label><input id="scheduled_wake_end" type="time" value="09:00"></div>
</div></div></section>
<footer class="action-bar"><div class="action-inner"><p class="status" id="save_status" role="status" aria-live="polite">Unsaved changes</p><button class="primary" type="submit">Save and restart</button></div></footer>
</form>
</div>
<script>
function emptyWidget(){return{type:'disabled',ttc:{route_id:'',direction_id:'',stop_id:'',route_label:'',stop_label:'',destination_label:''}}}
let savedConfig=null,csrfToken='';
const csrfHeader='X-TransitInk-CSRF',accessHeader='X-TransitInk-Access';
const portalAccessToken=typeof location==='undefined'?'':location.pathname.split('/').filter(Boolean)[0]||'';
const localCatalog={ttcIndex:null,providers:{},promises:{}};
let widgetDrafts=Array.from({length:4},()=>emptyWidget());
let expandedSlot=0;
const requestVersion=[0,0,0,0];
const catalogKeys=['ttcRoutes','ttcDirections','ttcStops'];
function emptyCatalogEntry(){return{status:'idle',items:[],error:''}}
function emptyCatalogState(){return Object.fromEntries(catalogKeys.map(key=>[key,emptyCatalogEntry()]))}
const catalogState=Array.from({length:4},()=>emptyCatalogState());
const widgetErrors=['','','',''];
let firstWidgetValidation=null;
function byId(id){return document.getElementById(id)}
function escapeHtml(value){return String(value??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]))}
function timeToMinutes(value){const match=/^(\d{2}):(\d{2})$/.exec(value||'');if(!match)return-1;const hours=Number(match[1]),minutes=Number(match[2]);return hours<24&&minutes<60?hours*60+minutes:-1}
function minutesToTime(value,fallback){const minutes=Number.isInteger(Number(value))?Number(value):fallback,safe=minutes>=0&&minutes<1440?minutes:fallback;return`${String(Math.floor(safe/60)).padStart(2,'0')}:${String(safe%60).padStart(2,'0')}`}
function syncPowerFields(){const scheduled=byId('sleep_enabled').checked&&byId('scheduled_wake_enabled').checked;byId('scheduled_wake_start').disabled=!scheduled;byId('scheduled_wake_end').disabled=!scheduled;byId('sleep_maintenance_hours').disabled=scheduled}
function portalFetch(path,options={}){const headers={...(options.headers||{})};if(portalAccessToken)headers[accessHeader]=portalAccessToken;return fetch(path,{...options,headers})}
function api(path,options){return portalFetch(path,options).then(async response=>{if(!response.ok)throw new Error(await response.text());return response.json()})}
function loadedEntry(items){return{status:'loaded',items:Array.isArray(items)?items:[],error:''}}
function resetCatalog(slot,...keys){keys.forEach(key=>catalogState[slot][key]=emptyCatalogEntry())}
async function loadTtcIndex(){const index=await api('/assets/catalog/current/ttc/index.json',{cache:'no-cache'});if(index?.schema_version!==1||!index.bus?.ttc?.routes)throw new Error('Invalid TTC catalog');localCatalog.ttcIndex=index}
async function loadProviderCatalog(provider){if(localCatalog.providers[provider])return localCatalog.providers[provider];if(localCatalog.promises[provider])return localCatalog.promises[provider];const revision=localCatalog.ttcIndex?.revision||'';localCatalog.promises[provider]=api(`/assets/catalog/current/ttc/stops-ttc.json?revision=${encodeURIComponent(revision)}`).then(result=>{if(result?.schema_version!==1||result.revision!==revision||!result.routes)throw new Error('Stop catalog mismatch');localCatalog.providers[provider]=result;delete localCatalog.promises[provider];return result}).catch(error=>{delete localCatalog.promises[provider];throw error});return localCatalog.promises[provider]}
function selectTab(name){['wifi','widgets','power'].forEach(tab=>{const selected=tab===name;byId(`tab_${tab}`).setAttribute('aria-selected',String(selected));byId(`panel_${tab}`).hidden=!selected});if(name==='widgets')ensureCatalogForSlot(expandedSlot)}
function typeLabel(type){return{disabled:'Disabled',ttc_eta:'TTC arrivals'}[type]||'Incomplete'}
function widgetSummary(widget){if(widget.type==='disabled')return'No arrivals shown';if(widget.type==='ttc_eta')return[widget.ttc.route_label||widget.ttc.route_id,widget.ttc.stop_label].filter(Boolean).join(' · ')||'Choose a TTC stop';return'Incomplete'}
function selectedOption(value,label){return value&&label?`<option value="${escapeHtml(value)}" selected>${escapeHtml(label)}</option>`:''}
function optionRows(items,value){return(items||[]).map(item=>`<option value="${escapeHtml(item.id)}" ${item.id===value?'selected':''} data-direction="${escapeHtml(item.direction_id||'')}" data-route-id="${escapeHtml(item.route_id||'')}" data-destination="${escapeHtml(item.destination_label||item.destination_label_tc||'')}">${escapeHtml(item.label||item.label_tc||item.id)}</option>`).join('')}
function selectField(id,label,source,value,currentLabel,change,placeholder='Select'){const entry=source||emptyCatalogEntry(),items=entry.items||[],loading=entry.status==='loading',failed=entry.status==='error',waiting=entry.status==='idle'&&placeholder!=='Select',empty=entry.status==='loaded'&&items.length===0,disabled=loading||failed||waiting||empty,prompt=loading?`Loading ${label}…`:failed?'Could not load options':empty?'No options':placeholder,hasCurrent=(items||[]).some(item=>item.id===value);return`<div class="field"><label for="${id}">${label}</label><select id="${id}" onchange="${change}" ${disabled?'disabled':''}><option value="">${prompt}</option>${hasCurrent?'':selectedOption(value,currentLabel)}${optionRows(items,value)}</select></div>`}
function searchableRouteField(id,label,source,value,currentLabel,inputHandler){const entry=source||emptyCatalogEntry(),items=entry.items||[],loading=entry.status==='loading',failed=entry.status==='error',waiting=entry.status==='idle',empty=entry.status==='loaded'&&items.length===0,disabled=loading||failed||waiting||empty,prompt=loading?`Loading ${label}…`:failed?'Could not load routes':empty?'No routes':'Enter route number',options=items.map(item=>`<option value="${escapeHtml(item.id)}">${escapeHtml(item.label||item.label_tc||item.id)}</option>`).join('');return`<div class="field"><label for="${id}">${label}</label><input id="${id}" type="search" list="${id}_options" value="${escapeHtml(currentLabel||value)}" placeholder="${escapeHtml(prompt)}" onchange="${inputHandler}" autocomplete="off" ${disabled?'disabled':''}><datalist id="${id}_options">${options}</datalist><p class="helper">Type a route number, then pick a direction and stop.</p></div>`}
function ttcDirectionValue(ttc){return ttc.route_id&&ttc.direction_id?`${ttc.route_id}:${ttc.direction_id}`:''}
function ttcRouteItems(){return Object.keys(localCatalog.ttcIndex?.bus?.ttc?.routes||{}).sort((a,b)=>a.localeCompare(b,undefined,{numeric:true})).map(id=>({id,label:id}))}
function ttcDirectionItems(routeLabel){return(localCatalog.ttcIndex?.bus?.ttc?.routes?.[routeLabel]||[]).map(item=>({id:item.stop_key,route_id:item.route_id,direction_id:item.direction_id,label:item.destination_label||item.stop_key,destination_label:item.destination_label||''}))}
function clearTtcAfterRoute(slot){const ttc=widgetDrafts[slot].ttc;ttc.route_id='';ttc.direction_id='';ttc.stop_id='';ttc.stop_label='';ttc.destination_label='';resetCatalog(slot,'ttcDirections','ttcStops')}
function clearTtcAfterDirection(slot){const ttc=widgetDrafts[slot].ttc;ttc.stop_id='';ttc.stop_label='';resetCatalog(slot,'ttcStops')}
function setWidgetType(slot,type){requestVersion[slot]++;widgetDrafts[slot]=emptyWidget();widgetDrafts[slot].type=type;catalogState[slot]=emptyCatalogState();widgetErrors[slot]='';renderWidgetCards();ensureCatalogForSlot(slot)}
function setTtcRouteSearch(slot,input){requestVersion[slot]++;const ttc=widgetDrafts[slot].ttc;ttc.route_label=(input.value||'').trim().toUpperCase();clearTtcAfterRoute(slot);widgetErrors[slot]='';renderWidgetCards();ensureCatalogForSlot(slot)}
function setTtcDirection(slot,select){requestVersion[slot]++;const ttc=widgetDrafts[slot].ttc;const option=select.selectedOptions[0];const parts=(select.value||'').split(':');ttc.route_id=option?.dataset?.routeId||parts[0]||'';ttc.direction_id=option?.dataset?.direction||parts[1]||'';ttc.destination_label=option?.dataset?.destination||option?.textContent?.trim()||'';if(!ttc.route_label)ttc.route_label=ttc.route_id;clearTtcAfterDirection(slot);widgetErrors[slot]='';renderWidgetCards();ensureCatalogForSlot(slot)}
function setTtcStop(slot,select){const ttc=widgetDrafts[slot].ttc;ttc.stop_id=select.value;ttc.stop_label=select.selectedOptions[0]?.textContent?.trim()||'';widgetErrors[slot]='';renderWidgetCards()}
function expandWidgetCard(slot){expandedSlot=slot;renderWidgetCards();ensureCatalogForSlot(slot)}
function moveWidget(slot,delta){const target=slot+delta;if(target<0||target>3)return;[widgetDrafts[slot],widgetDrafts[target]]=[widgetDrafts[target],widgetDrafts[slot]];[catalogState[slot],catalogState[target]]=[catalogState[target],catalogState[slot]];[widgetErrors[slot],widgetErrors[target]]=[widgetErrors[target],widgetErrors[slot]];expandedSlot=target;renderWidgetCards();ensureCatalogForSlot(expandedSlot)}
function renderWidgetFields(slot){const widget=widgetDrafts[slot],catalog=catalogState[slot];let html=`<div class="field full"><label for="widget_type_${slot}">Widget type</label><select id="widget_type_${slot}" onchange="setWidgetType(${slot},this.value)"><option value="disabled" ${widget.type==='disabled'?'selected':''}>Disabled</option><option value="ttc_eta" ${widget.type==='ttc_eta'?'selected':''}>TTC arrivals</option></select></div>`;if(widget.type==='ttc_eta'){html+=searchableRouteField(`ttc_route_${slot}`,'Route',catalog.ttcRoutes,widget.ttc.route_label||widget.ttc.route_id,widget.ttc.route_label||widget.ttc.route_id,`setTtcRouteSearch(${slot},this)`);html+=selectField(`ttc_direction_${slot}`,'Direction',catalog.ttcDirections,ttcDirectionValue(widget.ttc),widget.ttc.destination_label,`setTtcDirection(${slot},this)`,'Choose a route first');html+=selectField(`ttc_stop_${slot}`,'Stop',catalog.ttcStops,widget.ttc.stop_id,widget.ttc.stop_label,`setTtcStop(${slot},this)`,'Choose a direction first')}return html}
function renderWidgetCards(){byId('widget_cards').innerHTML=widgetDrafts.map((widget,slot)=>{const expanded=slot===expandedSlot;return`<article class="widget-card" data-expanded="${expanded}"><div class="widget-head"><button class="widget-toggle" type="button" onclick="expandWidgetCard(${slot})" aria-expanded="${expanded}"><span class="widget-title">Slot ${slot+1} · ${typeLabel(widget.type)}</span><span class="widget-summary">${escapeHtml(widgetSummary(widget))}</span></button><div class="order-actions"><button class="secondary" type="button" onclick="moveWidget(${slot},-1)" ${slot===0?'disabled':''}>Up</button><button class="secondary" type="button" onclick="moveWidget(${slot},1)" ${slot===3?'disabled':''}>Down</button></div></div>${expanded?`<div class="widget-body"><div class="widget-controls">${renderWidgetFields(slot)}</div><p class="widget-preview">Preview: ${escapeHtml(widgetSummary(widget))}</p><p class="widget-error" id="widget_error_${slot}" role="alert">${escapeHtml(widgetErrors[slot]||'')}</p></div>`:''}</article>`}).join('')}
async function loadStopsForSlot(slot){const entry=catalogState[slot].ttcStops;if(entry.status==='loading'||entry.status==='loaded')return;entry.status='loading';renderWidgetCards();const token=++requestVersion[slot];try{await loadProviderCatalog('ttc');if(token!==requestVersion[slot])return;entry.status='idle';renderWidgetCards();ensureCatalogForSlot(slot)}catch(error){if(token!==requestVersion[slot])return;entry.status='error';widgetErrors[slot]=`Stop catalog failed: ${error.message}`;renderWidgetCards()}}
function ensureCatalogForSlot(slot){if(slot!==expandedSlot||!localCatalog.ttcIndex)return;const widget=widgetDrafts[slot],catalog=catalogState[slot];let changed=false;if(widget.type!=='ttc_eta'){if(changed)renderWidgetCards();return}if(catalog.ttcRoutes.status==='idle'){catalog.ttcRoutes=loadedEntry(ttcRouteItems());changed=true}if(widget.ttc.route_label&&catalog.ttcDirections.status==='idle'){catalog.ttcDirections=loadedEntry(ttcDirectionItems(widget.ttc.route_label));changed=true}if(widget.ttc.route_id&&widget.ttc.direction_id&&catalog.ttcStops.status==='idle'){if(!localCatalog.providers.ttc)return loadStopsForSlot(slot);const key=`${widget.ttc.route_id}:${widget.ttc.direction_id}`;catalog.ttcStops=loadedEntry((localCatalog.providers.ttc.routes?.[key]||[]).map(item=>({id:item.id,label:item.label||item.id})));changed=true}if(changed)renderWidgetCards()}
function validateWidgetDrafts(){let valid=true;firstWidgetValidation=null;widgetDrafts.forEach((widget,slot)=>{let message='',fieldId='';if(widget.type==='ttc_eta'){if(!(widget.ttc.route_label||widget.ttc.route_id))fieldId=`ttc_route_${slot}`;else if(!(widget.ttc.route_id&&widget.ttc.direction_id))fieldId=`ttc_direction_${slot}`;else if(!widget.ttc.stop_id)fieldId=`ttc_stop_${slot}`;if(fieldId)message='Finish route, direction, and stop.'}widgetErrors[slot]=message;if(message){valid=false;if(!firstWidgetValidation)firstWidgetValidation={slot,fieldId}}});renderWidgetCards();return valid}
function collectConfig(){return{schema_version:3,wifi_ssid:byId('wifi_ssid').value.trim(),wifi_password:byId('wifi_password').value,sleep_enabled:byId('sleep_enabled').checked,wake_duration_minutes:Number(byId('wake_duration_minutes').value||5),sleep_maintenance_hours:Number(byId('sleep_maintenance_hours').value||0),scheduled_wake_enabled:byId('scheduled_wake_enabled').checked,scheduled_wake_start_minutes:timeToMinutes(byId('scheduled_wake_start').value),scheduled_wake_end_minutes:timeToMinutes(byId('scheduled_wake_end').value),widgets:widgetDrafts.map(widget=>{const out={type:widget.type};if(widget.type==='ttc_eta')out.ttc={...widget.ttc};return out})}}
function applyConfig(cfg){savedConfig=cfg;csrfToken=cfg.csrf_token||'';byId('current_ssid').textContent=cfg.wifi_ssid||'Not set';byId('password_status').textContent=cfg.wifi_password_set?'Saved password':'No saved password';byId('wifi_ssid').value=cfg.wifi_ssid||'';byId('wifi_password').value='';byId('sleep_enabled').checked=cfg.sleep_enabled!==false;byId('wake_duration_minutes').value=cfg.wake_duration_minutes||5;byId('sleep_maintenance_hours').value=cfg.sleep_maintenance_hours??12;byId('scheduled_wake_enabled').checked=cfg.scheduled_wake_enabled===true;byId('scheduled_wake_start').value=minutesToTime(cfg.scheduled_wake_start_minutes,480);byId('scheduled_wake_end').value=minutesToTime(cfg.scheduled_wake_end_minutes,540);syncPowerFields();widgetDrafts=Array.from({length:4},(_,slot)=>cfg.widgets?.[slot]?{...emptyWidget(),...cfg.widgets[slot],ttc:{...emptyWidget().ttc,...(cfg.widgets[slot].ttc||{})}}:emptyWidget());byId('top_facts').textContent=`Firmware ${cfg.firmware_version||'—'}`}
async function scanWifi(){byId('wifi_scan_status').textContent='Scanning…';try{const result=await api('/api/wifi/scan');const networks=result.networks||result||[];byId('wifi_scan_status').textContent=networks.length?`Found ${networks.length} networks`:'No networks found';if(networks.length){const datalist=document.getElementById('wifi_ssid_list')||Object.assign(document.createElement('datalist'),{id:'wifi_ssid_list'});datalist.innerHTML=networks.map(item=>`<option value="${escapeHtml(item.ssid||item)}"></option>`).join('');byId('wifi_ssid').setAttribute('list','wifi_ssid_list');if(!datalist.parentElement)document.body.appendChild(datalist)}}catch(error){byId('wifi_scan_status').textContent=`Scan failed: ${error.message}`}}
async function saveConfig(event){event.preventDefault();if(!byId('wifi_ssid').value.trim()){selectTab('wifi');byId('save_status').textContent='Enter a Wi-Fi network name.';byId('wifi_ssid').focus();return}if(byId('scheduled_wake_enabled').checked){const start=timeToMinutes(byId('scheduled_wake_start').value),end=timeToMinutes(byId('scheduled_wake_end').value);if(start<0||end<0||start===end){selectTab('power');byId('save_status').textContent='Choose different daily wake start and end times.';return}}if(!validateWidgetDrafts()){expandedSlot=firstWidgetValidation.slot;renderWidgetCards();selectTab('widgets');byId('save_status').textContent='Finish widget settings.';setTimeout(()=>byId(firstWidgetValidation.fieldId)?.focus(),0);return}const button=event.submitter||document.querySelector('.primary');button.disabled=true;byId('save_status').textContent='Saving…';try{const response=await portalFetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json',[csrfHeader]:csrfToken},body:JSON.stringify(collectConfig())});const body=await response.text();if(!response.ok)throw new Error(body);byId('save_status').textContent=body||'Saved. Device will restart.'}catch(error){byId('save_status').textContent=`Save failed: ${error.message}`;button.disabled=false}}
async function loadPortal(){const[cfg]=await Promise.all([api('/api/config',{cache:'no-store'}),loadTtcIndex()]);applyConfig(cfg);renderWidgetCards();ensureCatalogForSlot(expandedSlot);const required=widgetDrafts.some(widget=>widget.type==='ttc_eta');if(required)loadProviderCatalog('ttc').then(()=>ensureCatalogForSlot(expandedSlot)).catch(()=>{})}
loadPortal().catch(error=>{byId('save_status').textContent=`Failed to load settings: ${error.message}`});
</script>
</body>
</html>
)HTML";
