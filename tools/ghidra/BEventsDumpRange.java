// Board-events agent: decompiles every function whose entry lies in a hex
// range, read-only. Args: OUT_PATH LO HI (hex). Output: one block per
// function, headed "//---- NAME @ ADDR".
//@category Bejeweled3Access
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;

import java.io.PrintWriter;

public class BEventsDumpRange extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] a = getScriptArgs();
        PrintWriter out = new PrintWriter(a[0], "UTF-8");
        long lo = Long.parseLong(a[1], 16), hi = Long.parseLong(a[2], 16);
        DecompInterface decomp = new DecompInterface();
        decomp.openProgram(currentProgram);
        FunctionIterator it = currentProgram.getFunctionManager().getFunctions(toAddr(lo), true);
        while (it.hasNext()) {
            Function f = it.next();
            if (f.getEntryPoint().getOffset() > hi) break;
            DecompileResults res = decomp.decompileFunction(f, 30, monitor);
            out.println("//---- " + f.getName() + " @ " + f.getEntryPoint());
            out.println(res.decompileCompleted() ? res.getDecompiledFunction().getC() : "// decompile failed");
            out.flush();
        }
        out.close();
    }
}
