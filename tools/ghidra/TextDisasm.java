// Prints the disassembly of the function containing each address, plus every
// reference to the function's entry point (callers, vtable slots). Used for the
// text-drawing research (docs/re/text-drawing.md) to confirm calling
// conventions (ret N, register arguments) that decompilation hides.
// Args: OUT path, then hex addresses. Read-only.
//@category Bejeweled3Access
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.symbol.Reference;

import java.io.PrintWriter;

public class TextDisasm extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        PrintWriter out = new PrintWriter(args[0], "UTF-8");
        for (int i = 1; i < args.length; i++) {
            Address addr = toAddr(args[i]);
            Function f = getFunctionContaining(addr);
            out.println("//==== " + args[i]);
            if (f == null) {
                out.println("// no function");
                continue;
            }
            out.println("// function " + f.getName() + " @ " + f.getEntryPoint());
            for (Reference ref : getReferencesTo(f.getEntryPoint())) {
                Function from = getFunctionContaining(ref.getFromAddress());
                out.println("// ref from " + ref.getFromAddress() + " " + ref.getReferenceType()
                        + (from != null ? " in " + from.getName() : ""));
            }
            InstructionIterator it = currentProgram.getListing().getInstructions(f.getBody(), true);
            int n = 0;
            while (it.hasNext() && n++ < 400) {
                Instruction ins = it.next();
                out.println(ins.getAddress() + "  " + ins);
            }
        }
        out.close();
    }
}
