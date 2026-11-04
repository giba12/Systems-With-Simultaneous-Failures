// Checks the JavaScript port against numbers reported by the C++ project. Run: node test_engine.js (exit code 1 on failure)
const E=require('./engine.js');
const near=(a,b,t=1e-9)=>Math.abs(a-b)<t;
let failures=0;
const log=(n,ok,extra='')=>{ if(!ok) failures++; console.log((ok?'PASS ':'FAIL ')+n+' '+extra); };
const sat=E.SYSTEMS[0], fig=E.SYSTEMS[1];
const exp=E.makeModel('exp',0);
let a=E.analyse(sat,exp);
log('sat W all',JSON.stringify(a.W[63])==='[1,6,12,8,0,0,0]',JSON.stringify(a.W[63]));
log('sat phi calls 64',a.phiCalls===64);
log('sat mttf 0.7',near(a.mttf[63],0.7),a.mttf[63]);
log('sat m62 8/15',near(a.mttf[62],8/15),a.mttf[62]);
log('sig',JSON.stringify(a.sig[63].map(v=>+v.toFixed(3)))==='[0,0.2,0.4,0.4,0,0]',JSON.stringify(a.sig[63]));
log('min path sets 8',a.minimal.length===8,a.minimal.join(','));
log('operational 27',a.mttf.filter((m,x)=>sat.phi(x)).length===27);
log('state 62 W',JSON.stringify(a.W[62])==='[1,4,4,0,0,0]');
log('m(P1,C1 failed)=5/12',near(a.mttf[63&~1&~4],5/12),a.mttf[63&~1&~4]);
log('m(P1,P2)=0',a.mttf[60]===0);
const f=E.analyse(fig,exp);
log('fig1 W',JSON.stringify(f.W[7])==='[1,3,1,0]',JSON.stringify(f.W[7]));
log('fig1 mttf 7/6',near(f.mttf[7],7/6),f.mttf[7]);
// common cause numbers from the report
const ser=E.seriesOf(6);
for(const [t,b,expSat,expRatio] of [['lsh',0.5,0.914,3.20],['bfr',0.5,0.760,1.50]]){
  const m=E.makeModel(t,b); const s=E.analyse(sat,m).mttf[63], r=E.analyse(ser,m).mttf[63];
  console.log(t,b,'sat',s.toFixed(3),'series',r.toFixed(4),'ratio',(s/r).toFixed(2),'(expect',expSat,expRatio+')');
}
const ex=E.analyse(ser,exp).mttf[63]; console.log('exp series',ex.toFixed(4),'ratio',(0.7/ex).toFixed(2),'(expect 4.20)');
// pair equal across models
const pair=E.makeSystem?null:null;
// repair: paper Fig 1 costs
const costs={comp:1/30,sys:1,set:0};
const tc=E.thresholdCurve(fig,exp,f.mttf,costs);
console.log('fig1 threshold ltmc',tc.points.map(p=>p.ev.ltmc.toFixed(5)).join(' '),' expect',(97/105).toFixed(5),(11/15).toFixed(5),(1/10).toFixed(5));
const opt=E.optimalPolicy(fig,exp,costs); console.log('fig1 optimal',opt.ltmc.toFixed(5),opt.rounds.map(r=>r.ltmc.toFixed(4)).join('>'));
// satellite policies
const c2={comp:0.1,sys:1,set:0.05};
const tcs=E.thresholdCurve(sat,exp,a.mttf,c2);
console.log('sat threshold',tcs.points.map(p=>p.ev.ltmc.toFixed(4)).join(' '),'best',tcs.best);
console.log('sat r',[1,2,3,4].map(r=>E.evalPolicy(sat,exp,E.rOutOfN(sat,r),c2).ltmc.toFixed(4)).join(' '),'corr',E.evalPolicy(sat,exp,E.correctivePolicy(sat),c2).ltmc.toFixed(4));
const os=E.optimalPolicy(sat,exp,c2); console.log('sat optimal',os.ltmc.toFixed(4),os.rounds.length);
// monte carlo
let s=0,N=40000; for(let i=0;i<N;i++) s+=E.simulateLife(sat,exp,63); console.log('MC exp',(s/N).toFixed(3));
const lsh=E.makeModel('lsh',0.5); s=0; for(let i=0;i<N;i++) s+=E.simulateLife(sat,lsh,63); console.log('MC lsh',(s/N).toFixed(3),'exact',E.analyse(sat,lsh).mttf[63].toFixed(3));
const h=[0,0,0,0,0,0,0]; for(let i=0;i<N;i++) h[E.deathFlip(sat,63)]++; console.log('flip hist',h.map(v=>(v/N).toFixed(3)).join(' '));
// all systems valid: monotone, phi(0)=0, phi(all)=1
for(const S of E.SYSTEMS){ let ok=!S.phi(0)&&S.phi(S.all); for(let x=0;x<=S.all;x++) for(let i=0;i<S.n;i++) if((x>>i)&1 && S.phi(x&~(1<<i)) && !S.phi(x)) ok=false; const an=E.analyse(S,exp); log('valid '+S.id,ok,'mttf '+an.mttf[S.all].toFixed(4)+' min '+an.minimal.length); }
// beta extremes
for(const t of ['lsh','bfr']) for(const b of [0.01,1]){ const m=E.makeModel(t,b); const v=E.analyse(sat,m).mttf[63]; console.log(t,b,v.toFixed(4)); }
if (failures) { console.log(failures + " check(s) FAILED"); process.exitCode = 1; }
