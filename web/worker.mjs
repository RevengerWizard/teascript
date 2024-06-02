
import Module from './tea.js';

const modp = Module({
    noInitialRun: true,

    stdout(s) {
        self.postMessage({type: 'out', out: s});
    },

    stderr(e) {
        self.postMessage({type: 'err', err: e});
    }
});

const run = async (str) => {
    const mod = await modp;

    mod.callMain(['-e', str]);

    self.postMessage({type: 'stop'});
};

self.onmessage = async ({data: {type, ...data}}) => {
    if (type === 'run') {
        self.postMessage({type: 'start'});
        await run(data.code);
    } else {
        throw new Error(`type not known: ${type}`);
    }
};
