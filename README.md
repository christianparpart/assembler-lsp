# Assembler-LSP

### TODO Checklist

- [ ] AST (SSA) must have exact source location information to allow document modifications (LSP)
- [ ] design Instruction/register definition table (including instruction extensions) to satisfy the below requirements
- [ ] initial instruction/register database for x86-64 (including extensions: sse..avx512)
- [ ] parse ASM (intel style, nasm) into SSA
- [ ] natspec comment to inform LSP what instruction and instruction extensions should be allowed.
- [ ] diagnostics: syntax checking
- [ ] diagnostics: warn on undefined register content
- [ ] Typed function calls via some comment hint-syntax (natspec/doxygen-style); It should be possible to know what is input and output, (ideally including stack layuout).
- [ ] semantic highlighting: hovering registers should only show thos locations that share the same register value (via SSA analysis)
- [ ] symbolic renames of labels and data symbols
- [ ] symbolic renames registers: this requires rearranging other registers to satisfy this rename.
- [ ] goto to definition for label: jump to label definition
- [ ] goto to definition for register: jump to instruction that produces the output of that register
- [ ] autocompletion based on instruction set definitions; also parameters should be validly auto-completable.
- [ ] hover information on instruction: instr docs for this specific use of instruction
- [ ] hover information on register: where was it defined
- [ ] hover information on jump label: also show byte-distance

### Long term ideas

TBH, theoretically one could write an object file writer based on the SSA AST
the `libasm` has constructed.
