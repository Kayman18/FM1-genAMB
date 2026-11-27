/* Exercise the recovered editor's actual snapshot importer with a MIDI transport double. */
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const html = fs.readFileSync(__dirname + '/../../web-editor/index.html','utf8');
const js = html.match(/<script>([\s\S]*?)<\/script>/)[1].replace(/\bbind\(\);\s*$/, '');
const nodes = new Map();
const env = {
    document: {querySelector(selector) {
        if (!nodes.has(selector)) nodes.set(selector, {});
        return nodes.get(selector);
    }},
    messages: []
};
vm.createContext(env);
vm.runInContext(js,env);
vm.runInContext(`
    acked = async (cmd,args) => { messages.push([cmd,...args]); };
    setMeta = async () => {};
    requestState = async () => {};
`,env);
(async () => {
    for (const count of [167,172,174,189,201]) for (const object of [false,true]) {
        const params=Array.from(vm.runInContext('PARAMS',env));
        const values=params.slice(0,count).map(p=>p.def);
        for(const id of [47,50,53]){
            assert.equal(params[id].hi,24);
            values[id]=count>=189?24:count===174?17:15;
        }
        if(count>=172)values.splice(167,5,45,60,75,81,92);
        if(count===174){values[92]=12;values[172]=-83;values[173]=32;}
        if(count>=189){values[188]=39;values[172]=81;values[176]=23;}
        if(count===201)values.splice(189,12,14,-80,115,6,24,60,80,4,9,100,72,2);
        const wanted=params.map((p,i)=>i<count?values[i]:p.def);
        if(count<189){
            for(const id of [92,172,173])wanted[id]=params[id].def;
            if(count===174)for(const id of [47,50,53])wanted[id]=0;
        }
        const oldKeys={92:'P_FMPATCH',172:'P_FMDEPTH',173:'P_FMALGO'};
        const snapshot=object?Object.fromEntries(values.map((v,i)=>[
            count<189 && oldKeys[i]?oldKeys[i]:params[i].key,v])):values;
        env.messages.length=0;
        env.file={text:async()=>JSON.stringify({format:'genAMB-FM1-Preset',params:snapshot,
            source:count>=189?'genAMB Nature VA8':'genAMB RC'})};
        await vm.runInContext('loadPresetFile(file)',env);
        const blocks=[];
        for (const [cmd,lo,hi,length,...bytes] of env.messages) {
            assert.equal(cmd,4); assert.equal(lo+128*hi,blocks.length); assert.equal(bytes.length,length*2);
            for(let i=0;i<bytes.length;i+=2)blocks.push(bytes[i]+128*bytes[i+1]-8192);
        }
        assert.deepEqual(blocks,wanted);
    }
    console.log('PASS legacy and VA snapshots, array and keyed formats, all 201 parameters');
})().catch(e=>{console.error(e);process.exitCode=1;});
