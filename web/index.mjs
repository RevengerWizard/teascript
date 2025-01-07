
import {default as ace} from 'https://esm.run/ace-builds';

const worker = () => {
    return new Worker(new URL('./worker.mjs', import.meta.url), {type: 'module'});
};

let workers = [];

for (let i = 0; i < 4; i++) {
    workers.push(worker());
}

let i = 0;
const run = (code) => new Promise((good, bad) => {
    i += 1;
    const w = workers[i % workers.length];
    workers[i % workers.length] = worker();
    let out = '';
    let err = '';
    let done = false;
    const kill = () => {
        w.terminate();
        if (!done) {
            good([null, 'timeout']);
            done = true;
        }
    };
    w.onmessage = ({data: {type, ...data}}) => {
        if (type === 'start') {
            setTimeout(kill, 1000);
        } else if (type === 'stop') {
            if (!done) {
                good([out, err]);
                done = true;
            }
        } else if (type === 'out') {
            out += String.fromCharCode(data.out);
        } else if (type === 'err') {
            err += String.fromCharCode(data.err);
        } else {
            throw new Error(`type not known: ${type}`);
        }
    };
    w.onerror = () => {
        if (!done) {
            bad(null);
            done = true;
        }
    };
    w.postMessage({type: 'run', code: code});
});

const div = (e) => {
    const d = document.createElement('div');
    e.appendChild(d);
    d.style.width = '100%';
    d.style.height = '100%';
    d.style.boxSizing = 'border-box';
    return d;
};

const hdiv = (e) => {
    const d = div(e);
    d.style.display = 'flex';
    d.style.flexDirection = 'column';
    d.style.alignItems = 'stretch';
    d.style.justifyContent = 'stretch';
    return d;
};

const vdiv = (e) => {
    const d = div(e);
    d.style.display = 'flex';
    d.style.flexDirection = 'row';
    d.style.alignItems = 'stretch';
    d.style.justifyContent = 'stretch';
    return d;
};

const main = async () => {
    const main = div(document.body);
    const body = hdiv(vdiv(main));
    const v1 = div(body);
    v1.style.padding = '1em';
    const v2 = vdiv(body);
    const v3 = div(v2);
    const v4 = div(v2);
    v2.style.padding = '1em';
    v2.style.fontFamily = 'monospace';
    const edit = ace.edit();
    edit.container.style.height = '100%';
    v1.appendChild(edit.container);
    edit.setValue(localStorage.getItem('teascript.src'));
    const change = async () => {
        const src = edit.getValue();
        localStorage.setItem('teascript.src', src);
        const [out, err] = await run(src).catch(e => console.error(e));
        if (out != null) {
            v3.innerText = out;
        }
        if (err != null) {  
            v4.innerText = err;
        }
    };
    edit.on('change', change);
    change();
};

main().catch(e => console.error(e));
