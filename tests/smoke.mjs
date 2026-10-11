import assert from 'node:assert/strict';
import {browserSession} from '../tools/browser.mjs';
const s=await browserSession();
try{
 assert.equal(await s.page.evaluate(()=>crossOriginIsolated),true);
 const script=`set -eu
command -v sh cc ar make guc-fetch gucman
mkdir -p /tmp/core-test
cd /tmp/core-test
printf '#include <stdio.h>\nint main(void){puts("MINIMAL-COMPILER-PASS");return 0;}\n' > hello.c
printf 'hello: hello.o\n\tcc hello.o -o hello\nhello.o: hello.c\n\tcc -c hello.c -o hello.o\n' > Makefile
make -j2
./hello
ar rcs libhello.a hello.o
ar t libhello.a
gucman repo list
printf persistent > /home/user/minimal-proof
`;
 const log=await s.command(script);assert.match(log,/MINIMAL-COMPILER-PASS/);assert.match(log,/hello.o/);console.log(log);
 await s.page.reload();await s.page.waitForFunction(()=>window.shell?.ready);
 assert.match(await s.command('set -eu\ntest "$(cat /home/user/minimal-proof)" = persistent\n/tmp/core-test/hello\nmake --version'),/MINIMAL-COMPILER-PASS/);
 console.log('PASS: fresh boot, Make parallel recipe, compiler, archive, repository listing, persistence and reload');
}finally{await s.close();}
