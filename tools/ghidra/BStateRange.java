// Decompiles every function whose entry lies in the given address ranges
// (read-only), for the board-state research (docs/re/board-state.md).
// Args: OUT_FILE START END [START END ...] (hex addresses).
//@category Bejeweled3Access
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;

import java.io.PrintWriter;

public class BStateRange extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        PrintWriter out = new PrintWriter(args[0], "UTF-8");
        DecompInterface decomp = new DecompInterface();
        decomp.openProgram(currentProgram);
        for (int i = 1; i + 1 < args.length; i += 2) {
            Address start = toAddr(args[i]);
            Address end = toAddr(args[i + 1]);
            FunctionIterator it = currentProgram.getFunctionManager().getFunctions(start, true);
            while (it.hasNext()) {
                Function f = it.next();
                if (f.getEntryPoint().compareTo(end) >= 0) break;
                DecompileResults res = decomp.decompileFunction(f, 60, monitor);
                out.println("//---- " + f.getName() + " @ " + f.getEntryPoint());
                out.println(res.decompileCompleted() ? res.getDecompiledFunction().getC() : "// decompile failed");
                out.flush();
            }
        }
        out.close();
    }
}
