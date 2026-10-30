// Decompiles functions for reverse-engineering notes.
// Args: list of hex addresses. For a data address (e.g. a vtable), every
// function that references it is decompiled; for a code address, the function
// containing it is decompiled. Output goes to the file named by the first
// argument (a path; cmd.exe splits arguments on "=").
//@category Bejeweled3Access
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;

import java.io.PrintWriter;
import java.util.LinkedHashSet;
import java.util.Set;

public class DecompileRefs extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        PrintWriter out = new PrintWriter(args[0], "UTF-8");
        DecompInterface decomp = new DecompInterface();
        decomp.openProgram(currentProgram);
        for (int i = 1; i < args.length; i++) {
            Address addr = toAddr(args[i]);
            Set<Function> funcs = new LinkedHashSet<>();
            Function direct = getFunctionContaining(addr);
            if (direct != null) {
                funcs.add(direct);
            } else {
                for (Reference ref : getReferencesTo(addr)) {
                    Function f = getFunctionContaining(ref.getFromAddress());
                    if (f != null) funcs.add(f);
                }
            }
            out.println("//==== " + args[i] + " : " + funcs.size() + " function(s)");
            for (Function f : funcs) {
                DecompileResults res = decomp.decompileFunction(f, 60, monitor);
                out.println("//---- " + f.getName() + " @ " + f.getEntryPoint());
                out.println(res.decompileCompleted() ? res.getDecompiledFunction().getC() : "// decompile failed");
            }
        }
        out.close();
    }
}
