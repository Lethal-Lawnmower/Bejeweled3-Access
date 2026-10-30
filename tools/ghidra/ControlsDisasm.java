// Prints the disassembly of the function containing each address (read-only).
// Args: output file path, then hex addresses. Used by the controls RE pass.
//@category Bejeweled3Access
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;

import java.io.PrintWriter;

public class ControlsDisasm extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        PrintWriter out = new PrintWriter(args[0], "UTF-8");
        for (int i = 1; i < args.length; i++) {
            Address addr = toAddr(args[i]);
            Function f = getFunctionContaining(addr);
            if (f == null) {
                out.println("//==== " + args[i] + " : no function");
                continue;
            }
            out.println("//==== " + f.getName() + " @ " + f.getEntryPoint());
            InstructionIterator it = currentProgram.getListing().getInstructions(f.getBody(), true);
            while (it.hasNext()) {
                Instruction ins = it.next();
                out.println(ins.getAddress() + "  " + ins.toString());
            }
        }
        out.close();
    }
}
