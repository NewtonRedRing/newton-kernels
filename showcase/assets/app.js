/* Original NewtonKernels visualization. Data are sourced from verified repo JSON. */
'use strict';

const modules = [
  {name:'Core',target:'core',number:'01',title:'CPU detection & dispatch',features:'CPU feature discovery · target-safe dispatch · portable execution',comparison:false,note:'Correctness/API only; no isolated external performance test.'},
  {name:'Memory',target:'memory',number:'02',title:'Mapped & aligned memory',features:'Read-only mapping · aligned buffers · allocation control',comparison:false,note:'Need file-I/O throughput, footprint and allocation benchmarks.'},
  {name:'SIMD',target:'simd',number:'03',title:'Accelerated byte operations',features:'Scalar · AVX2 · AVX-512 · runtime dispatch',comparison:true,competitor:'Optimized STL std::count',note:'Byte-count benchmark only; no broad SIMD suite yet.'},
  {name:'Parsing',target:'parsing',number:'04',title:'CSV & delimited records',features:'Quoted CSV · borrowed unquoted records · field decoding',comparison:true,competitor:"Vince's CSV Parser",note:'One-thread, in-memory CSV processing; no file-backed parallel comparison.'},
  {name:'Index',target:'index',number:'05',title:'Search & inverted index',features:'Posting lists · Boolean intersection · phrase search',comparison:false,note:'Internal algorithm comparison exists; external match not yet run.'},
  {name:'Analytics',target:'analytics',number:'06',title:'Aggregation & selection',features:'Group counts · joins · exact percentile selection',comparison:false,note:'Internal sort/selection comparison exists; external match not yet run.'},
  {name:'Graph',target:'graph',number:'07',title:'Shortest path & CSR',features:'Packed CSR · binary or radix queues · reusable workspaces',comparison:true,competitor:'Boost.Graph Dijkstra',note:'Target queries; excludes graph construction. Workspace policies differ.'},
  {name:'Parallel',target:'parallel',number:'08',title:'Bounded parallel execution',features:'Thread workers · task futures · parallel scheduling',comparison:false,note:'Need scaling, tail latency and scheduling benchmark.'}
];

const matrix = [
  ['Correctness / output equality','Verified','Verified','Verified','Unit tests only'],
  ['Latency (same-input external)','Measured','Measured','Measured','Not measured'],
  ['Throughput (derived from time)','Derived','Derived','Derived','Not measured'],
  ['Peak process RSS','Captured*','Captured*','Captured*','Not measured'],
  ['Latency spread / repeats','Recorded','Recorded','Recorded','Not measured'],
  ['Input size sensitivity','2 sizes','2 sizes','2 sizes','Not measured'],
  ['Parallel scaling / thread counts','Not measured','Not measured','Not measured','Not measured'],
  ['Cold vs warm cache','Not measured','Not measured','Not measured','Not measured'],
  ['Allocator / peak allocation bytes','Not measured','Not measured','Not measured','Not measured'],
  ['Energy / power draw','Not measured','Not measured','Not measured','Not measured'],
  ['CPU cycles / IPC / branch misses','Not measured','Not measured','Not measured','Not measured'],
  ['Cache misses / bandwidth counters','Not measured','Not measured','Not measured','Not measured'],
  ['Build time / binary size','Not measured','Not measured','Not measured','Not measured'],
  ['Cross-OS external benchmarking','Not measured','Not measured','Not measured','Not measured'],
  ['Feature parity / API completeness','Narrow subset','Narrow subset','Narrow subset','Not assessed'],
  ['End-to-end application speed','Not measured','Not measured','Not measured','Not measured']
];
const $ = id => document.getElementById(id);
const names = {scan:'SIMD byte count',csv:'Quoted CSV decode',graph:'Shortest-path queries'};
const competitorNames = {stl:'C++ standard library',boost:'Boost.Graph',vince:"Vince's CSV Parser"};
const format = (n,d=3) => new Intl.NumberFormat('en-US',{maximumFractionDigits:d,minimumFractionDigits:d}).format(n);
const calculate = (row,metric) => {
  if(metric==='latency') return {newton:row.newton_median_ms,competitor:row.competitor_median_ms,unit:'ms',direction:'low',decimals:4};
  if(metric==='throughput') {
    const units = row.operation==='scan' ? row.size / 1_000_000 : (row.operation==='csv' ? row.size : row.queries);
    const multiplier = row.operation==='scan' ? 1_000 : 1_000;
    return {newton:units*multiplier/row.newton_median_ms,competitor:units*multiplier/row.competitor_median_ms,unit:row.operation==='scan'?'MB/s':row.operation==='csv'?'rows/s':'queries/s',direction:'high',decimals:1};
  }
  const a = row.samples.newton.map(x=>x.peak_process_rss_kib/1024).sort((x,y)=>x-y);
  const b = row.samples[row.competitor].map(x=>x.peak_process_rss_kib/1024).sort((x,y)=>x-y);
  return {newton:a[Math.floor(a.length/2)],competitor:b[Math.floor(b.length/2)],unit:'MiB*',direction:'none',decimals:2};
};
function makeNode(tag, cls, text){const el=document.createElement(tag);if(cls)el.className=cls;if(text!==undefined)el.textContent=text;return el;}
function renderModules(){const root=$('module-grid');modules.forEach(item=>{
  const card=makeNode('article','module');
  const top=makeNode('div','module-top');top.append(makeNode('span','module-no',item.number));top.append(makeNode('span',item.comparison?'module-status verified':'module-status pending',item.comparison?'Measured externally':'Not externally measured'));card.append(top);
  card.append(makeNode('p','module-tag','NEWTONKERNELS::'+item.target.toUpperCase()));
  card.append(makeNode('h3','',item.title)); card.append(makeNode('p','module-features',item.features));
  if(item.comparison) card.append(makeNode('p','module-opponent','Compared with '+item.competitor));
  card.append(makeNode('p','module-note',item.note));root.append(card);
});}
function renderMatrix(){const root=$('metric-matrix');matrix.forEach((row)=>{const tr=makeNode('tr');row.forEach((cell,i)=>{const td=makeNode(i===0?'th':'td');if(i===0)td.scope='row';else td.className=cell==='Measured'||cell==='Verified'||cell==='Recorded'||cell==='Derived'||cell.endsWith('*')||cell.includes('sizes')?'status-present':'status-pending';td.textContent=cell;tr.append(td);});root.append(tr);});}
function addBar(root,label,value,metric,max,side){const item=makeNode('div','bar-item');const heading=makeNode('div','bar-heading');heading.append(makeNode('span','bar-label',label),makeNode('strong','bar-value',format(value,metric.decimals)+' '+metric.unit));item.append(heading);const track=makeNode('div','bar-track');track.setAttribute('role','meter');track.setAttribute('aria-label',label+' '+format(value,metric.decimals)+' '+metric.unit);track.setAttribute('aria-valuemin','0');track.setAttribute('aria-valuemax',String(max));track.setAttribute('aria-valuenow',String(value));const fill=makeNode('div','bar-fill '+side);fill.style.width=(max>0?Math.max(0.3,100*value/max):0)+'%';track.append(fill);item.append(track);root.append(item);}
function renderCards(data){const metric=$('metric').value;const category=$('workload').value;const outcome=$('winner').value;const root=$('benchmark-list');root.replaceChildren();let count=0;
  for(const row of data.results){if(category!=='all' && row.operation!==category)continue;const r=calculate(row,metric);const ahead=r.direction==='none'?'unranked':Math.abs(r.newton-r.competitor)<1e-10?'equal':r.direction==='low'?(r.newton<r.competitor?'newton':'competitor'):(r.newton>r.competitor?'newton':'competitor');if(outcome!=='all'&&ahead!==outcome)continue;count++;
    const card=makeNode('article','result-card');const top=makeNode('div','result-top');const heading=makeNode('div');heading.append(makeNode('span','result-eyebrow',row.operation.toUpperCase()+' / MATCHED INPUT'),makeNode('h3','',names[row.operation]));heading.append(makeNode('p','result-size','Workload: '+row.size.toLocaleString()+(row.operation==='scan'?' bytes':row.operation==='csv'?' rows':' vertices')+(row.operation==='graph'?' · '+row.queries+' queries':'')));
    top.append(heading);const result=makeNode('span',ahead==='competitor'?'outcome competitor-wins':ahead==='newton'?'outcome newton-wins':'outcome neutral',ahead==='newton'?'NewtonKernels lower latency':ahead==='competitor'?'Competitor ahead':ahead==='equal'?'Equal':'RSS: no library winner assigned');if(metric==='throughput' && ahead==='newton')result.textContent='NewtonKernels higher throughput'; if(metric==='rss')result.textContent='RSS: process totals only';top.append(result);card.append(top);
    const bars=makeNode('div','bars');const max=Math.max(r.newton,r.competitor);addBar(bars,'NewtonKernels',r.newton,r,max,'newton');addBar(bars,competitorNames[row.competitor],r.competitor,r,max,'competitor');card.append(bars);
    const notes=makeNode('div','result-meta');const magnitude=(row.competitor_median_ms/row.newton_median_ms);notes.append(makeNode('span','ratio','Latency ratio '+format(magnitude,2)+'×'+(magnitude<1?' (competitor faster)':'')));notes.append(makeNode('span','verified-label',row.verified_equal_output?'✓ Digest matched':'Unverified'));notes.append(makeNode('span','checksum','Digest '+row.checksum));card.append(notes);
    const details=makeNode('details','raw-detail');details.append(makeNode('summary','','View reproducibility, sample spread & memory'));const info=makeNode('div','detail-grid');
    const pairs=[['Input seed',String(row.seed)],['Repetitions',row.rounds+' processes × '+row.repetitions+' repetitions'],['Newton process RSS (median)',format(calculate(row,'rss').newton,2)+' MiB'],['Competitor process RSS (median)',format(calculate(row,'rss').competitor,2)+' MiB'],['Newton per-process timing medians',row.samples.newton.map(x=>format(x.median_ms,4)+' ms').join(' · ')],['Competitor per-process timing medians',row.samples[row.competitor].map(x=>format(x.median_ms,4)+' ms').join(' · ')],['Competitor revision',row.competitor_revision]];
    pairs.forEach(([key,value])=>{const div=makeNode('div','detail-entry');div.append(makeNode('span','detail-key',key),makeNode('span','detail-value',value));info.append(div);});details.append(info);card.append(details);root.append(card);
  }
  if(!count){root.append(makeNode('div','empty','No matching measured results. Change a filter, or inspect the capability coverage below.'));}
  $('shown-count').textContent=count+' of '+data.results.length+' verified comparisons';
}
async function boot(){renderModules();renderMatrix();try{const res=await fetch('data/results.json',{cache:'no-store'});if(!res.ok)throw new Error('HTTP '+res.status);const data=await res.json();if(!data.results?.length)throw new Error('No recorded data');if(data.results.some(r=>!r.verified_equal_output))throw new Error('Unverified comparison found');const top=Math.max(...data.results.map(r=>r.speedup));$('max-speedup').textContent=format(top,2);$('verified-count').textContent=String(data.results.length);['metric','workload','winner'].forEach(x=>$(x).addEventListener('change',()=>renderCards(data)));renderCards(data);}catch(err){$('benchmark-list').replaceChildren(makeNode('p','error','Results could not be loaded. Serve this folder via HTTP (python3 -m http.server), or inspect the committed results JSON. '+err.message));}}
boot();
