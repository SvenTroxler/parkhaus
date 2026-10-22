#pragma once
// Weboberflaechen: Schranken (/) und Kasse (/kasse)

static const char SCHRANKEN_SEITE[] = R"HTML(<!doctype html>
<html lang="de"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Parkhaus – Schranken</title>
<style>
body{font-family:system-ui,sans-serif;margin:0;background:#f2f4f7;color:#1d2433}
header{background:#1d3557;color:#fff;padding:14px 20px;display:flex;justify-content:space-between;align-items:center;flex-wrap:wrap;gap:8px}
h1{margin:0;font-size:1.3em}
main{max-width:1100px;margin:0 auto;padding:16px}
.spalten{display:grid;grid-template-columns:repeat(auto-fit,minmax(320px,1fr));gap:16px}
section{background:#fff;border-radius:10px;padding:16px;box-shadow:0 1px 3px #0002;margin-bottom:16px}
h2{margin:0 0 12px;font-size:1.15em}h3{margin:16px 0 8px;font-size:.95em;color:#555}
.display{background:#111;color:#ffd166;font-family:ui-monospace,monospace;border-radius:8px;padding:14px;min-height:90px}
.display .gross{font-size:2.2em;font-weight:700;color:#7CFC8a}
.display .gross.voll{color:#ff5c5c}
.display .text{font-size:1.1em;margin-top:6px}
.display.fehler .text{color:#ff7b7b}.display.ok .text{color:#7CFC8a}
.schranke{display:flex;align-items:center;gap:12px;margin:14px 0}
.pfosten{width:14px;height:44px;background:#555;border-radius:3px}
.balken{width:150px;height:12px;background:repeating-linear-gradient(90deg,#d62828 0 20px,#fff 20px 40px);border:1px solid #333;border-radius:3px;transform-origin:left center;transition:transform .4s}
.balken.offen{transform:rotate(-80deg)}
.status{font-weight:700}
.zeile{display:flex;gap:8px;flex-wrap:wrap;align-items:center;margin:6px 0}
button{padding:8px 14px;border:0;border-radius:6px;background:#1d3557;color:#fff;cursor:pointer;font-size:.95em}
button.gruen{background:#2a9d4a}button.rot{background:#c0392b}button.grau{background:#888}button.gelb{background:#e9a400}
input{padding:8px;border:1px solid #bbb;border-radius:6px;font-size:.95em;width:9em}
.hinweis{font-size:.9em;min-height:1.2em;color:#555}
#personal .alarm{background:#fde2e1;border:2px solid #c0392b;border-radius:8px;padding:12px;margin-bottom:10px;animation:blink 1s infinite alternate}
@keyframes blink{to{border-color:#fde2e1}}
header a{color:#fff}
</style></head><body>
<header><h1>Parkhaus – Schranken</h1><a href="/kasse">Zur Kasse</a></header>
<main>
<div class="spalten" id="seiten"></div>
<section id="personal"><h2>Personal</h2><div id="alarme"></div>
<div class="hinweis" id="keinAlarm">Keine Hilfe-Anfragen.</div></section>
</main>
<script>
const SEITEN={ein:'Einfahrt',aus:'Ausfahrt'};
const t=()=>Math.floor(Date.now()/1000);
async function api(pfad,params={}){params.t=t();
  const r=await fetch(pfad+'?'+new URLSearchParams(params),{method:pfad=='/api/status'?'GET':'POST'});return r.json();}
const esc=s=>String(s).replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));

document.getElementById('seiten').innerHTML=Object.keys(SEITEN).map(id=>`
<section><h2>${SEITEN[id]}</h2>
  <div class="display" id="d-${id}">${id=='ein'?`<div>Freie Plätze</div><div class="gross" id="f-${id}">–</div>`:''}<div class="text" id="m-${id}"></div></div>
  <div class="schranke"><div class="pfosten"></div><div class="balken" id="b-${id}"></div><span class="status" id="s-${id}"></span></div>
  <h3>Kamera-Simulation</h3>
  <div class="zeile"><input id="k-${id}" placeholder="z.B. ZH 123456" onkeydown="if(event.key=='Enter')kamera('${id}')">
  <button onclick="kamera('${id}')">Kennzeichen senden</button>
  <button class="grau" onclick="kamera('${id}',true)">Nicht erkannt</button></div>
  <h3>Am Fahrzeug</h3>
  <div class="zeile"><button class="gelb" onclick="hilfe('${id}')">Hilfe-Taste</button>
  <button class="gruen" onclick="durchfahrt('${id}')">Auto durchgefahren</button></div>
  <div class="hinweis" id="h-${id}"></div>
</section>`).join('');

function hinweis(id,text){const h=document.getElementById('h-'+id);h.textContent=text||'';clearTimeout(h._t);h._t=setTimeout(()=>h.textContent='',5000);}
async function kamera(id,leer){const f=document.getElementById('k-'+id);
  const r=await api('/api/kamera',{seite:id,kz:leer?'':f.value});if(!r.ok)hinweis(id,r.text);f.value='';status();}
async function hilfe(id){await api('/api/hilfe',{seite:id});status();}
async function durchfahrt(id){const r=await api('/api/durchfahrt',{seite:id});hinweis(id,r.text);status();}
async function manuell(id){const f=document.getElementById('p-'+id);
  const r=await api('/api/manuell',{seite:id,kz:f.value});
  if(!r.ok&&r.text.startsWith('Ungültig'))document.getElementById('pf-'+id).textContent=r.text;status();}
async function beenden(id){await api('/api/hilfe_beenden',{seite:id});status();}

let alarmeVorher='';
async function status(){
  let s;try{s=await api('/api/status');}catch(e){return;}
  for(const id in SEITEN){const x=s[id];
    const d=document.getElementById('d-'+id);d.className='display '+x.typ;
    document.getElementById('m-'+id).textContent=x.meldung;
    if(id=='ein'){const f=document.getElementById('f-ein');
      f.textContent=s.frei<0?'–':(s.frei==0?'BESETZT':s.frei);f.className='gross'+(s.frei==0?' voll':'');}
    document.getElementById('b-'+id).className='balken'+(x.offen?' offen':'');
    document.getElementById('s-'+id).textContent=x.offen?'OFFEN':'ZU';}
  // Personal-Benachrichtigung (AK 6.2)
  const offen=Object.keys(SEITEN).filter(id=>s[id].hilfe);
  const schluessel=offen.join(',');
  if(schluessel!==alarmeVorher){alarmeVorher=schluessel;
    document.getElementById('alarme').innerHTML=offen.map(id=>`<div class="alarm">
      <b>Hilfe angefordert: ${SEITEN[id]}</b><div class="zeile">Kennzeichen manuell erfassen:
      <input id="p-${id}" placeholder="z.B. BE 98765" onkeydown="if(event.key=='Enter')manuell('${id}')">
      <button onclick="manuell('${id}')">Erfassen</button><button class="grau" onclick="beenden('${id}')">Abbrechen</button></div>
      <div class="hinweis" id="pf-${id}"></div></div>`).join('');}
  document.getElementById('keinAlarm').hidden=offen.length>0;
}
status();setInterval(status,500);
</script></body></html>
)HTML";


static const char KASSE_SEITE[] = R"HTML(<!doctype html>
<html lang="de"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Parkhaus – Kasse</title>
<style>
body{font-family:system-ui,sans-serif;margin:0;background:#f2f4f7;color:#1d2433}
header{background:#1d3557;color:#fff;padding:14px 20px;display:flex;justify-content:space-between;align-items:center;flex-wrap:wrap;gap:8px}
h1{margin:0;font-size:1.3em}
main{max-width:1000px;margin:0 auto;padding:16px}
section{background:#fff;border-radius:10px;padding:16px;margin-bottom:16px;box-shadow:0 1px 3px #0002}
h2{margin:0 0 12px;font-size:1.1em}
table{width:100%;border-collapse:collapse;font-size:.95em}
th,td{text-align:left;padding:7px 6px;border-bottom:1px solid #e3e6ea}
th{color:#555;font-weight:600}
button{padding:8px 14px;border:0;border-radius:6px;background:#1d3557;color:#fff;cursor:pointer;font-size:.95em}
button.gruen{background:#2a9d4a}button.rot{background:#c0392b}button.grau{background:#888}
input{padding:8px;border:1px solid #bbb;border-radius:6px;font-size:.95em}
.tag{display:inline-block;padding:2px 8px;border-radius:10px;font-size:.85em}
.ok{background:#d4f4dd;color:#17692e}.offen{background:#fde2e1;color:#a52a1f}.man{background:#fff3cd;color:#7a5a00}
.frei{font-size:1.1em}
#zahlung{border:2px solid #1d3557}
.betrag{font-size:2em;font-weight:700;margin:8px 0}
.zeile{display:flex;gap:8px;flex-wrap:wrap;align-items:center;margin:8px 0}
#meldung{padding:10px;border-radius:6px;margin-bottom:12px}
.wrap{overflow-x:auto}
details summary{cursor:pointer;font-weight:600}
header a{color:#fff}
</style></head><body>
<header><h1>Parkhaus – Kasse</h1><a href="/">Zu den Schranken</a>
<div class="frei">Freie Plätze: <b id="frei">–</b> / <span id="kap">–</span></div></header>
<main>
<div id="meldung" hidden></div>

<section id="zahlung" hidden>
  <h2>Zahlung: <span id="zKz"></span></h2>
  <div>Parkdauer: <span id="zDauer"></span></div>
  <div class="betrag" id="zBetrag"></div>
  <div class="zeile" id="zArt">
    Zahlart: <button onclick="art('kreditkarte')">Kreditkarte</button>
    <button onclick="art('twint')">Twint</button>
    <button class="grau" onclick="schliessen()">Abbrechen</button>
  </div>
  <div class="zeile" id="zSim" hidden>
    <span id="zArtName"></span> (simuliert):
    <button class="gruen" onclick="zahlen('ok')">Zahlung erfolgreich</button>
    <button class="rot" onclick="zahlen('fehler')">Zahlung fehlgeschlagen</button>
    <button class="grau" onclick="schliessen()">Abbrechen</button>
  </div>
</section>

<section>
  <h2>Fahrzeuge im Parkhaus</h2>
  <div class="zeile"><input id="suche" placeholder="Kennzeichen suchen" oninput="zeichnen()"></div>
  <div class="wrap"><table><thead><tr><th>Kennzeichen</th><th>Einfahrt</th><th>Parkdauer</th><th>Gebühr</th><th>Status</th><th></th></tr></thead>
  <tbody id="liste"></tbody></table></div>
</section>

<section>
<details><summary>Testmodus</summary>
  <p>Einfahrtszeit eines Fahrzeugs zurücksetzen (TF-09, TF-20):</p>
  <div class="zeile"><input id="tKz" placeholder="Kennzeichen" size="10">
  <input id="tStd" type="number" min="0" value="1" style="width:5em"> h
  <input id="tMin" type="number" min="0" max="59" value="1" style="width:5em"> Min. vor jetzt
  <button onclick="testZeit()">Einfahrtszeit setzen</button></div>
</details>
</section>
</main>
<script>
let daten={fahrzeuge:[]},auswahl=null,zahlart=null;
const t=()=>Math.floor(Date.now()/1000);
async function api(pfad,params={},methode='GET'){
  params.t=t();
  const r=await fetch(pfad+'?'+new URLSearchParams(params),{method:methode});
  return r.json();
}
const chf=r=>'CHF '+(r/100).toFixed(2);
const zeit=s=>s?new Date(s*1000).toLocaleString('de-CH'):'–';
function dauer(sek){sek=Math.max(0,sek);const m=Math.floor(sek/60);return Math.floor(m/60)+' h '+String(m%60).padStart(2,'0')+' Min.';}
const esc=s=>String(s).replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));
function meldung(text,ok){const m=document.getElementById('meldung');m.textContent=text;m.className='tag '+(ok?'ok':'offen');m.hidden=false;clearTimeout(m._t);m._t=setTimeout(()=>m.hidden=true,6000);}
const GRUENDE={ungueltig:'Ungültiges Kennzeichen',unbekannt:'Fahrzeug nicht im Parkhaus',bereits_bezahlt:'Bereits bezahlt',zahlung_fehlgeschlagen:'Zahlung fehlgeschlagen – Status bleibt „nicht bezahlt“'};

async function laden(){
  try{daten=await api('/api/fahrzeuge');zeichnen();}catch(e){}
}
function zeichnen(){
  document.getElementById('frei').textContent=daten.frei;
  document.getElementById('kap').textContent=daten.kapazitaet;
  const q=document.getElementById('suche').value.toUpperCase().replace(/\s/g,'');
  const drin=daten.fahrzeuge.filter(f=>f.kz.replace(/\s/g,'').includes(q)).sort((a,b)=>a.ein-b.ein);
  document.getElementById('liste').innerHTML=drin.map(f=>`<tr>
    <td><b>${esc(f.kz)}</b> ${f.manuell?'<span class="tag man">manuell erfasst</span>':''}</td>
    <td>${zeit(f.ein)}</td><td>${dauer(daten.jetzt-f.ein)}</td><td>${chf(f.gebuehr)}</td>
    <td>${f.bezahlt?'<span class="tag ok">bezahlt</span>':'<span class="tag offen">nicht bezahlt</span>'}</td>
    <td>${f.bezahlt?'':`<button onclick="oeffnen('${esc(f.kz)}')">Bezahlen</button>`}</td></tr>`).join('')
    ||'<tr><td colspan="6">Keine Fahrzeuge</td></tr>';
  if(auswahl){
    const f=daten.fahrzeuge.find(x=>x.kz==auswahl);
    if(!f||f.bezahlt){if(!f)schliessen();return;}
    document.getElementById('zDauer').textContent=dauer(daten.jetzt-f.ein);
    document.getElementById('zBetrag').textContent=chf(f.gebuehr);
  }
}
function oeffnen(kz){auswahl=kz;zahlart=null;
  document.getElementById('zKz').textContent=kz;
  document.getElementById('zahlung').hidden=false;
  document.getElementById('zArt').hidden=false;document.getElementById('zSim').hidden=true;zeichnen();}
function schliessen(){auswahl=null;document.getElementById('zahlung').hidden=true;}
function art(a){zahlart=a;document.getElementById('zArtName').textContent=a=='twint'?'Twint':'Kreditkarte';
  document.getElementById('zArt').hidden=true;document.getElementById('zSim').hidden=false;}
async function zahlen(ergebnis){
  const r=await api('/api/zahlen',{kz:auswahl,art:zahlart,ergebnis},'POST');
  if(r.ok){meldung('Zahlung bestätigt: '+chf(r.betrag)+' – '+auswahl+' ist bezahlt',true);schliessen();}
  else{meldung(GRUENDE[r.grund]||r.grund,false);if(r.grund=='zahlung_fehlgeschlagen'){document.getElementById('zArt').hidden=false;document.getElementById('zSim').hidden=true;}}
  laden();
}
async function antwort(r,text){meldung(r.ok?text:(GRUENDE[r.grund]||r.grund),r.ok);laden();}
function minuten(){return (+document.getElementById('tStd').value)*60+(+document.getElementById('tMin').value);}
async function testZeit(){antwort(await api('/api/test/einfahrtszeit',{kz:tKz.value,minuten:minuten()},'POST'),'Einfahrtszeit gesetzt');}
laden();setInterval(laden,1000);
</script></body></html>
)HTML";
