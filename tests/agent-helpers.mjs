// Poll asynchronous guest filesystem reads explicitly. A Promise-valued
// waitForFunction predicate can settle false without waiting for a later read.
export async function pollPage(page,predicate,arg,{timeout=30000}={}){
 const deadline=Date.now()+timeout;
 for(;;){const value=await page.evaluate(predicate,arg);if(value)return value;if(Date.now()>=deadline)throw Error('Timed out polling guest state: '+predicate.toString());await page.waitForTimeout(100);}
}
