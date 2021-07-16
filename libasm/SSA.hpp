#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace asmlsp
{

class BasicBlock;
class Instr;
class TerminateInstr;

class FunctionDefinition; 		// user defined function
class InstructionDefinition;
class InstructionVisitor;

enum class LiteralType
{
	Void,
	Int,
	UInt,
	String,
};

class Value
{
public:
	Value(LiteralType ty, std::string name);
	virtual ~Value() = default;
	LiteralType type() const { return type_; }
	const std::string& name() const { return name_; }
	void addUse(Instr* user);
	void removeUse(Instr* user);
	bool isUsed() const { return !uses_.empty(); }
	const std::vector<Instr*>& uses() const { return uses_; }
	size_t useCount() const { return uses_.size(); }
	void replaceAllUsesWith(Value* newUse);

private:
	LiteralType type_;
	std::string name_;
	std::vector<Instr*> uses_;  //! list of instructions that <b>use</b> this value.
};

class Constant: public Value
{
public:
	Constant(LiteralType ty, std::string name): Value(ty, std::move(name)) {}
};

template <typename T, const LiteralType Ty>
class ConstantValue: public Constant
{
public:
	ConstantValue(const T& value, std::string name = ""):
		Constant(Ty, name), value_(std::move(value)) {}

	T get() const { return value_; }

private:
	T value_;
};

using ConstantInt = ConstantValue<int64_t, LiteralType::Int>;
using ConstantUInt = ConstantValue<uint64_t, LiteralType::UInt>;

class Instr: public Value
{
  public:
    Instr(LiteralType ty, std::vector<Value*> ops = {}, std::string name = "");
    ~Instr();

    /**
     * Retrieves parent basic block this instruction is part of.
     */
    BasicBlock* getBasicBlock() const { return basicBlock_; }

    /**
     * Read-only access to operands.
     */
    const std::vector<Value*>& operands() const { return operands_; }

    /**
     * Retrieves n'th operand at given \p index.
     */
    Value* operand(size_t index) const { return operands_[index]; }

    /**
     * Adds given operand \p value to the end of the operand list.
     */
    void addOperand(Value* value);

    /**
     * Sets operand at index \p i to given \p value.
     *
     * This operation will potentially replace the value that has been at index \p
     *i before,
     * properly unlinking it from any uses or successor/predecessor links.
     */
    Value* setOperand(size_t i, Value* value);

    /**
     * Replaces \p old operand with \p replacement.
     *
     * @param old value to replace
     * @param replacement new value to put at the offset of \p old.
     *
     * @returns number of actual performed replacements.
     */
    size_t replaceOperand(Value* old, Value* replacement);

    /**
     * Clears out all operands.
     *
     * @see addOperand()
     */
    void clearOperands();

    /**
     * Replaces this instruction with the given @p newInstr.
     *
     * @returns ownership of this instruction.
     */
    std::unique_ptr<Instr> replace(std::unique_ptr<Instr> newInstr);

    /**
     * Clones given instruction.
     *
     * This will not clone any of its operands but reference them.
     */
    virtual std::unique_ptr<Instr> clone() = 0;

    /**
     * Generic extension interface.
     *
     * @param v extension to pass this instruction to.
     *
     * @see InstructionVisitor
     */
    virtual void accept(InstructionVisitor& v) = 0;

  protected:
	BasicBlock* basicBlock_;
	std::vector<Value*> operands_;
};

/**
 * Creates a PHI (phoney) instruction.
 *
 * Creates a synthetic instruction that purely informs the target register
 * allocator to allocate the very same register for all given operands,
 * which is then used across all their basic blocks.
 */
class PhiNode: public Instr {
  public:
	PhiNode(const std::vector<Value*>& ops, const std::string& name);

	std::unique_ptr<Instr> clone() override;
	void accept(InstructionVisitor& v) override;
};

class TerminateInstr: public Instr
{
  protected:
	TerminateInstr(const TerminateInstr& v): Instr(v) {}

  public:
	TerminateInstr(const std::vector<Value*>& ops): Instr(LiteralType::Void, ops, "") {}
};

class CpuInstr: public Instr {
  public:
    CpuInstr(std::vector<Value*>& args, std::string name);
    CpuInstr(InstructionDefinition* callee, std::vector<Value*> args, std::string name);

    FunctionDefinition* callee() const { return (FunctionDefinition*)operand(0); }

    std::unique_ptr<Instr> clone() override;
    void accept(InstructionVisitor& v) override;
};

class CallInstr: public Instr {
  public:
    CallInstr(std::string _labelName, std::vector<Value*> args, std::string name);
    CallInstr(std::string _labelName, FunctionDefinition* _resolvedSymbol, std::vector<Value*> args, std::string name);

    FunctionDefinition* callee() const { return (FunctionDefinition*) operand(0); }
	//void setCallee(FunctionDefinition* _callee) { operands_[0] = _callee; }

    std::unique_ptr<Instr> clone() override;
    void accept(InstructionVisitor& v) override;
};

class BasicBlock: public Value
{
  public:
    BasicBlock(std::string name, Value& parent);
    ~BasicBlock();

    Value& parent() const { return parent_.get(); }
    void setParent(Value& _parent) { parent_ = _parent; }

    /*!
     * Retrieves the last terminating instruction in this basic block.
     *
     * This instruction must be a termination instruction, such as
     * a branching instruction or a function terminating instruction.
     *
     * @see BrInstr, CondBrInstr, MatchInstr, RetInstr
     */
    TerminateInstr* getTerminator() const;

    /**
     * Checks whether this BasicBlock is assured to terminate, hence, complete.
     *
     * This is either achieved by having a TerminateInstr at the end or a NativeCallback
     * that never returns.
     */
    bool isComplete() const;

    /**
     * Retrieves the linear ordered list of instructions of instructions in this
     * basic block.
     */
    std::vector<std::unique_ptr<Instr>>& instructions() { return code_; }
    Instr* instruction(size_t i) { return code_[i].get(); }

	Instr* front() const { return code_.front().get(); }
	Instr* back() const { return code_.back().get(); }

    size_t size() const { return code_.size(); }
    bool empty() const { return code_.empty(); }

    Instr* back(size_t sub) const
	{
        if (sub + 1 <= code_.size())
            return code_[code_.size() - (1 + sub)].get();
        else
            return nullptr;
    }

    /**
     * Appends a new instruction, \p instr, to this basic block.
     *
     * The basic block will take over ownership of the given instruction.
     */
    Instr* push_back(std::unique_ptr<Instr> instr);

    /**
     * Removes given instruction from this basic block.
     *
     * The basic block will pass ownership of the given instruction to the caller.
     * That means, the caller has to either delete \p childInstr or transfer it to
     * another basic block.
     *
     * @see push_back()
     */
    std::unique_ptr<Instr> remove(Instr* childInstr);

    /**
     * Replaces given @p oldInstr with @p newInstr.
     *
     * @return returns given @p oldInstr.
     */
    std::unique_ptr<Instr> replace(Instr* oldInstr, std::unique_ptr<Instr> newInstr);

    /**
     * Merges given basic block's instructions into this ones end.
     *
     * The passed basic block's instructions <b>WILL</b> be touched, that is
     * - all instructions will have been moved.
     * - any successor BBs will have been relinked
     */
    void merge_back(BasicBlock* bb);

    /**
     * Moves this basic block after the other basic block, \p otherBB.
     *
     * @param otherBB the future prior basic block.
     *
     * In a function, all basic blocks (starting from the entry block)
     * will be aligned linear into the execution segment.
     *
     * This function moves this basic block directly after
     * the other basic block, \p otherBB.
     *
     * @see moveBefore()
     */
    void moveAfter(const BasicBlock* otherBB);

    /**
     * Moves this basic block before the other basic block, \p otherBB.
     *
     * @see moveAfter()
     */
    void moveBefore(const BasicBlock* otherBB);

    /**
     * Tests whether or not given block is straight-line located after this block.
     *
     * @retval true \p otherBB is straight-line located after this block.
     * @retval false \p otherBB is not straight-line located after this block.
     *
     * @see moveAfter()
     */
    bool isAfter(const BasicBlock* otherBB) const;

    /**
     * Links given \p successor basic block to this predecessor.
     *
     * @param successor the basic block to link as an successor of this basic
     *                                    block.
     *
     * This will also automatically link this basic block as
     * future predecessor of the \p successor.
     *
     * @see unlinkSuccessor()
     * @see successors(), predecessors()
     */
    void linkSuccessor(BasicBlock* successor);

    /**
     * Unlink given \p successor basic block from this predecessor.
     *
     * @see linkSuccessor()
     * @see successors(), predecessors()
     */
    void unlinkSuccessor(BasicBlock* successor);

    /** Retrieves all predecessors of given basic block. */
    std::vector<BasicBlock*>& predecessors() { return predecessors_; }

    /** Retrieves all uccessors of the given basic block. */
    std::vector<BasicBlock*>& successors() { return successors_; }
    const std::vector<BasicBlock*>& successors() const { return successors_; }

    /** Retrieves all dominators of given basic block. */
    std::vector<BasicBlock*> dominators();

    /** Retrieves all immediate dominators of given basic block. */
    std::vector<BasicBlock*> immediateDominators();

    /**
     * Performs sanity checks on internal data structures.
     *
     * This call does not return any success or failure as every failure is
     * considered fatal and will cause the program to exit with diagnostics
     * as this is most likely caused by an application programming error.
     */
    void verify();

 private:
    void collectIDom(std::vector<BasicBlock*>& output);

 private:
    std::reference_wrapper<Value> parent_;
    std::vector<std::unique_ptr<Instr>> code_;
    std::vector<BasicBlock*> predecessors_;
    std::vector<BasicBlock*> successors_;

    friend class IRBuilder;
    friend class Instr;
};

}
