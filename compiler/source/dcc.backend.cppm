export module dcc.backend;

import std;
import dcc.ir;
import dcc.target;
import dcc.sm;

export namespace dcc::backend
{
    enum class ArtifactKind : std::uint8_t
    {
        LlvmIrText,
        AsmText,
        ObjectBytes,
        ExecutableBytes,
    };

    struct BackendDiagnostic
    {
        sm::SourceRange where;
        std::string message;
    };

    struct BackendArtifact
    {
        std::optional<std::string> llvm_ir_text;
        std::optional<std::string> asm_text;
        std::optional<std::vector<std::byte>> object_bytes;
        std::optional<std::vector<std::byte>> executable_bytes;
        std::vector<BackendDiagnostic> diagnostics;
    };

    struct BackendOptions
    {
        target::TargetConfig target;
        std::set<ArtifactKind> requested_artifacts;
        std::vector<std::string> additional_objects;
        std::vector<std::string> library_paths;
        std::vector<std::string> libraries;
        std::vector<std::string> linker_args;
        bool emit_debug_info{false};
    };

    class Backend
    {
    public:
        Backend() = default;
        virtual ~Backend() = default;
        Backend(Backend const&) = delete;
        Backend& operator=(Backend const&) = delete;

        [[nodiscard]] virtual std::string_view name() const = 0;
        [[nodiscard]] virtual std::set<ArtifactKind> supported_artifacts() const = 0;

        [[nodiscard]] virtual BackendArtifact emit(ir::IrModule const& module, BackendOptions const& options) = 0;
    };

} // namespace dcc::backend
