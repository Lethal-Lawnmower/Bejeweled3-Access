// Board-flow agent: decompiles every function whose entry lies in LO..HI
// (hex) and writes them to OUT_PATH, for offline grepping. Read-only.
// Args: OUT_PATH LO HI
//@category Bejeweled3Access
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;

import java.io.PrintWriter;

public class BFlowDumpRange extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] a = getScriptArgs();
        PrintWriter out = new PrintWriter(a[0], "UTF-8");
        long lo = Long.parseLong(a[1], 16), hi = Long.parseLong(a[2], 16);
        DecompInterface d = new DecompInterface();
        d.openProgram(currentProgram);
        FunctionIterator it = currentProgram.getFunctionManager().getFunctions(toAddr(lo), true);
        while (it.hasNext()) {
            Function f = it.next();
            if (f.getEntryPoint().getOffset() > hi) break;
            DecompileResults r = d.decompileFunction(f, 60, monitor);
            out.println("//---- " + f.getName() + " @ " + f.getEntryPoint());
            out.println(r.decompileCompleted() ? r.getDecompiledFunction().getC() : "// decompile failed");
        }
        out.close();
    }
}
