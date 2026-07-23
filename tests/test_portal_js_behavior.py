import subprocess
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def portal_script() -> str:
    source = (ROOT / "src/TransitInkPortalPage.cpp").read_text(encoding="utf-8")
    script = source.split("<script>", 1)[1].split("</script>", 1)[0]
    return script.split("loadPortal().catch", 1)[0]


class PortalJavaScriptBehaviorTests(unittest.TestCase):
    def test_lan_portal_fetches_include_session_access_token(self):
        script = portal_script()
        harness = r"""
let captured=null;
global.fetch=(path,options)=>{captured={path,options};return Promise.resolve({ok:true,json:async()=>({})})};
(async()=>{
  await portalFetch('/api/config',{headers:{Accept:'application/json'}});
  if(captured?.path!=='/api/config')throw new Error('wrong path');
  if(captured?.options?.headers?.['X-TransitInk-Access']!=='SESSION123')throw new Error('missing LAN access token');
  if(captured?.options?.headers?.Accept!=='application/json')throw new Error('existing headers overwritten');
  process.stdout.write('ok');
})().catch(error=>{console.error(error);process.exitCode=1});
"""
        completed = subprocess.run(
            ["node", "-e", "global.location={pathname:'/SESSION123'};" + script + harness],
            cwd=ROOT,
            text=True,
            capture_output=True,
            timeout=10,
        )
        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertEqual(completed.stdout, "ok")

    def test_ttc_catalog_labels_are_escaped_before_rendering(self):
        script = portal_script()
        harness = r"""
const elements={};
function element(id){return elements[id]||(elements[id]={id,value:'',checked:false,hidden:false,innerHTML:'',textContent:'',disabled:false,selectedOptions:[],setAttribute(){},insertAdjacentHTML(){},focus(){}})}
global.document={getElementById:element,querySelector(){return element('primary')},createElement(tag){return element(tag)},body:{appendChild(){}}};
global.fetch=()=>{throw new Error('loaded device catalog should not fetch here')};
localCatalog.ttcIndex={schema_version:1,revision:'fixture',bus:{ttc:{routes:{'506':[{stop_key:'506:0',route_id:'506',direction_id:'0',destination_label:'<img src=x onerror=alert(1)>'}]}}}};
localCatalog.providers.ttc={schema_version:1,revision:'fixture',routes:{'506:0':[{id:'8431',label:'College <img src=x onerror=alert(1)>',sequence:1}]}};
widgetDrafts[0]=emptyWidget();
widgetDrafts[0].type='ttc_eta';
widgetDrafts[0].ttc.route_label='506';
widgetDrafts[0].ttc.route_id='506';
widgetDrafts[0].ttc.direction_id='0';
expandedSlot=0;
ensureCatalogForSlot(0);
renderWidgetCards();
const markup=element('widget_cards').innerHTML;
if(markup.includes('<img src=x'))throw new Error('catalog string became executable HTML');
if(!markup.includes('&lt;img src=x onerror=alert(1)&gt;'))throw new Error('catalog string was not escaped');
process.stdout.write('ok');
"""
        completed = subprocess.run(
            ["node", "-e", script + harness],
            cwd=ROOT,
            text=True,
            capture_output=True,
            timeout=10,
        )
        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertEqual(completed.stdout, "ok")

    def test_ttc_route_direction_stop_selection_reaches_schema_v3_post(self):
        script = portal_script()
        harness = r"""
const elements={};
function element(id){return elements[id]||(elements[id]={id,value:'',checked:false,hidden:false,innerHTML:'',textContent:'',disabled:false,selectedOptions:[],setAttribute(name,value){this[name]=value},setCustomValidity(value){this.validationMessage=value},insertAdjacentHTML(){},focus(){}})}
global.document={getElementById:element,querySelector(){return element('primary')},createElement(tag){return element(tag)},body:{appendChild(){}}};
let postedConfig=null;
global.fetch=(path,options)=>{
  if(path==='/api/config'&&options?.method==='POST'){postedConfig=JSON.parse(options.body);return Promise.resolve({ok:true,text:async()=>'Settings saved. Device is restarting.'})}
  throw new Error(`unexpected request: ${path}`);
};
(async()=>{
  csrfToken='csrf';
  element('wifi_ssid').value='TransitInk';
  element('sleep_enabled').checked=true;
  element('wake_duration_minutes').value='5';
  element('sleep_maintenance_hours').value='12';
  element('scheduled_wake_enabled').checked=false;
  element('scheduled_wake_start').value='08:00';
  element('scheduled_wake_end').value='09:00';
  localCatalog.ttcIndex={schema_version:1,revision:'fixture',bus:{ttc:{routes:{'506':[{stop_key:'506:0',route_id:'506',direction_id:'0',destination_label:'Eastbound'}]}}}};
  localCatalog.providers.ttc={schema_version:1,revision:'fixture',routes:{'506:0':[{id:'8431',label:'College St at University Ave',sequence:1}]}};
  widgetDrafts=Array.from({length:4},()=>emptyWidget());
  widgetDrafts[1].type='ttc_eta';
  expandedSlot=1;
  renderWidgetCards();
  setTtcRouteSearch(1,{id:'ttc_route_1',value:'506',setAttribute(){},setCustomValidity(){}});
  setTtcDirection(1,{value:'506:0',selectedOptions:[{textContent:'Eastbound',dataset:{routeId:'506',direction:'0',destination:'Eastbound'}}]});
  setTtcStop(1,{value:'8431',selectedOptions:[{textContent:'College St at University Ave'}]});
  await saveConfig({preventDefault(){},submitter:element('submit')});
  const saved=postedConfig?.widgets?.[1];
  if(postedConfig?.schema_version!==3)throw new Error('schema_version was not 3');
  if('weather_location_tc' in postedConfig||'weather_location' in postedConfig)throw new Error('weather field was posted');
  if(saved?.type!=='ttc_eta')throw new Error('TTC widget type missing');
  if(saved.ttc?.route_id!=='506'||saved.ttc?.direction_id!=='0'||saved.ttc?.stop_id!=='8431')throw new Error('TTC identifiers missing');
  if(saved.ttc?.route_label!=='506'||saved.ttc?.destination_label!=='Eastbound'||saved.ttc?.stop_label!=='College St at University Ave')throw new Error('TTC labels missing');
  if(postedConfig.widgets.some(widget=>widget.type!=='disabled'&&widget.type!=='ttc_eta'))throw new Error('unexpected widget type');
  process.stdout.write('ok');
})().catch(error=>{console.error(error.message);process.exit(1)});
"""
        completed = subprocess.run(
            ["node", "-e", script + harness],
            cwd=ROOT,
            text=True,
            capture_output=True,
            timeout=10,
        )
        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertEqual(completed.stdout, "ok")

    def test_ttc_catalog_lifecycle_and_first_validation_focus(self):
        script = portal_script()
        harness = r"""
const elements={};
let focused='';
function element(id){return elements[id]||(elements[id]={id,value:'',checked:false,hidden:false,innerHTML:'',textContent:'',disabled:false,selectedOptions:[],setAttribute(name,value){this[name]=value},insertAdjacentHTML(){},focus(){focused=id;}})}
global.document={getElementById:element,querySelector(){return element('primary')},createElement(tag){return element(tag)},body:{appendChild(){}}};
global.fetch=()=>{throw new Error('local TTC catalog should satisfy this test')};
(async()=>{
  localCatalog.ttcIndex={schema_version:1,revision:'fixture',bus:{ttc:{routes:{'506':[{stop_key:'506:0',route_id:'506',direction_id:'0',destination_label:'Eastbound'}]}}}};
  localCatalog.providers.ttc={schema_version:1,revision:'fixture',routes:{'506:0':[{id:'8431',label:'College St at University Ave'}]}};
  widgetDrafts=Array.from({length:4},()=>emptyWidget());
  widgetDrafts[0].type='ttc_eta';
  expandedSlot=0;
  ensureCatalogForSlot(0);
  await new Promise(resolve=>setTimeout(resolve,0));
  if(catalogState[0].ttcRoutes.status!=='loaded'||catalogState[0].ttcRoutes.items[0]?.id!=='506')throw new Error('TTC routes were not loaded from local catalog');
  setTtcRouteSearch(0,{id:'ttc_route_0',value:'506',setAttribute(){},setCustomValidity(){}});
  await new Promise(resolve=>setTimeout(resolve,0));
  if(catalogState[0].ttcDirections.status!=='loaded'||catalogState[0].ttcDirections.items[0]?.id!=='506:0')throw new Error('TTC directions were not loaded');

  widgetDrafts=Array.from({length:4},()=>emptyWidget());
  widgetDrafts[2].type='ttc_eta';
  element('wifi_ssid').value='TransitInk';
  element('sleep_enabled').checked=true;
  element('wake_duration_minutes').value='5';
  element('sleep_maintenance_hours').value='12';
  element('scheduled_wake_enabled').checked=false;
  element('scheduled_wake_start').value='08:00';
  element('scheduled_wake_end').value='09:00';
  await saveConfig({preventDefault(){},submitter:element('submit')});
  await new Promise(resolve=>setTimeout(resolve,10));
  if(expandedSlot!==2)throw new Error('first invalid widget was not expanded');
  if(focused!=='ttc_route_2')throw new Error(`wrong focus: ${focused}`);
  if(!element('save_status').textContent.includes('Finish widget settings'))throw new Error('validation status missing');
  process.stdout.write('ok');
})().catch(error=>{console.error(error.message);process.exit(1)});
"""
        completed = subprocess.run(
            ["node", "-e", script + harness],
            cwd=ROOT,
            text=True,
            capture_output=True,
            timeout=10,
        )
        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertEqual(completed.stdout, "ok")

    def test_source_has_english_ttc_portal_and_no_removed_helpers(self):
        source = (ROOT / "src/TransitInkPortalPage.cpp").read_text(encoding="utf-8")
        self.assertIn('<html lang="en">', source)
        self.assertIn("TTC surface arrivals for your e-ink dashboard", source)
        self.assertIn("Configure up to four arrival slots. Each slot can show one TTC stop.", source)
        self.assertIn("schema_version:3", source)
        self.assertIn("ttc_eta", source)
        self.assertIn("loadTtcIndex", source)
        self.assertIn("/assets/catalog/current/ttc/index.json", source)
        self.assertIn("/assets/catalog/current/ttc/stops-ttc.json", source)
        self.assertNotIn("weather_location", source)
        self.assertNotIn("gmb", source.lower())
        self.assertNotIn("bus_eta", source)
        self.assertNotIn("mtr_eta", source)
        self.assertNotIn("journey_time", source)


if __name__ == "__main__":
    unittest.main()
