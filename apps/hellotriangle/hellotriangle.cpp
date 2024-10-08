#include <oscar/oscar.h>

#ifdef EMSCRIPTEN
#include <emscripten.h>
#endif
#include <TextEditor.h>
#include <lauxlib.h>
#include <luaconf.h>
#include <lua.h>
#include <lcode.h>
#include <lualib.h>
#include <oscar/Utils/Assertions.h>

#include <array>
#include <cstdlib>
#include <new>
#include <sstream>
#include <stdexcept>
#include <utility>

using namespace osc;

namespace
{
    constexpr CStringView c_gamma_correcting_vertex_shader_src = R"(
#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

void main()
{
    TexCoord = aTexCoord;
    gl_Position = vec4(aPos, 1.0);
}
)";
    constexpr CStringView c_gamma_correcting_fragment_shader_src = R"(
#version 330 core

uniform sampler2D uTexture;

in vec2 TexCoord;
out vec4 FragColor;

void main()
{
    // linear --> sRGB
    vec4 linearColor = texture(uTexture, TexCoord);
    FragColor = vec4(pow(linearColor.rgb, vec3(1.0/2.2)), linearColor.a);
}
)";

    namespace lua
    {
        // a LUA state (i.e. VM, stack, registry, etc.)
        class State final {
        public:
            State()
            {
                if (state_ == nullptr) {
                    throw std::bad_alloc{};
                }

                // install a C++ exception-based panic handler for the state
                //
                // (otherwise, lua will call `exit`, which will crash the entire
                //  application, rather than using the exception-handling fallback)
                lua_atpanic(state_, [](lua_State* state) -> int
                {
                    size_t len = 0;
                    throw std::runtime_error{lua_tolstring(state, -1, &len)};
                });
            }
            State(const State&) = delete;
            State(State&& tmp) noexcept :
                state_{std::exchange(tmp.state_, nullptr)}
            {}
            State& operator=(const State&) = delete;
            State& operator=(State&& tmp) noexcept
            {
                std::swap(tmp.state_, state_);
                return *this;
            }
            ~State() noexcept
            {
                if (state_) {
                    lua_close(state_);
                }
            }

            lua_State* get() { return state_; }
        private:
            // memory allocation function that the LUA implementation uses to allocate state
            // (e.g. memory used by the script)
            static void* state_memory_allocator(
                [[maybe_unused]] void* user_data,
                void* ptr,
                [[maybe_unused]] size_t old_size,
                size_t new_size)
            {
                // https://www.lua.org/manual/5.1/manual.html#lua_Alloc

                if (new_size == 0) {
                    std::free(ptr);  // NOLINT(cppcoreguidelines-owning-memory,cppcoreguidelines-no-malloc,hicpp-no-malloc)
                    return nullptr;
                }
                else {
                    return std::realloc(ptr, new_size);  // NOLINT(cppcoreguidelines-no-malloc,hicpp-no-malloc,cppcoreguidelines-owning-memory)
                }
            }

            lua_State* state_ = lua_newstate(state_memory_allocator, nullptr);
        };
        /*
        // custom `Lua_CFunction` implementation of string reversal
        int adamlib_reverse(lua_State *L) {
            size_t len = 0;
            const char *s = luaL_checklstring(L, 1, &len);
            luaL_Buffer b;
            luaL_buffinit(L, &b);
            while (len--) luaL_addchar(&b, s[len]);
            luaL_pushresult(&b);
            return 1;
        }

        static const luaL_Reg adamlib[] = {
            {"reverse", adamlib_reverse},
            {NULL, NULL}
        };

        void adamlib_createmetatable(lua_State* L)
        {
            lua_createtable(L, 0, 1);  // push empty table onto stack
            lua_pushliteral(L, "");    // push dummy string onto stack (we're setting the metatable for *all* strings)
            lua_pushvalue(L, -2);      // copy first table on stack and push copy onto stack
            lua_setmetatable(L, -2);   // pop copied table from stack and set it as metatable for dummy string
            lua_pop(L, 1);             // pop copied string
            lua_pushvalue(L, -2);      // push copy of top of stack before calling this function onto the stack
            lua_setfield(L, -2, "__index");  // set copy of top of stack's `__index` metamethod to the table created here and pop the top of the stack
            lua_pop(L, 1);                   // pop the table created here
        }

        // caller does:
        //
        // - lua_pushcfunction(state, luaopen_adamlib);
        // - lua_pushstring(state, name.c_str());
        // - lua_call(state, 1, 0);
        void luaopen_adamlib(lua_State* L)
        {
            luaL_register(L, "adam", adamlib);
            createmetatable(L);
            return 1;
        }
        */

        // returns a `std::string_view` of some string located at some stack index in
        // a lua state
        std::string_view tostringview(lua_State* state, int index)
        {
            size_t len = 0;
            const char* str = lua_tolstring(state, index, &len);  // note: may convert the number to a string inside the stack
            if (str) {
                return {str, len};
            }
            else {
                return {};
            }
        }

        int lua_load_cstringview(lua_State* L, CStringView view, const char* chunkname)
        {
            const auto reader = [](lua_State*, void* data, size_t* size) -> const char*
            {
                const auto next_view = std::exchange(*static_cast<CStringView*>(data), CStringView{});
                *size = next_view.size();
                return next_view.empty() ? nullptr : next_view.c_str();
            };
            return lua_load(L, reader, &view, chunkname);
        }

        [[maybe_unused]] std::string compile_and_print(CStringView chunk_name, CStringView source_code)
        {
            // create a blank state
            lua::State state;

            // TODO: create an environment in the state's global variable table
            // (e.g. `environments.someenv`) and give each environment whitelisted
            // functions, so that multiple environments can share a single VM

            // load the (safe) standard library symbols into the default globals table of the state
            struct NamedLuaCFunction final {
                CStringView name;
                lua_CFunction function;
            };
            constexpr auto standard_library_functions = std::to_array<NamedLuaCFunction>({
                // see also: `luaL_openlibs` to just open everything
                {"", luaopen_base},
                {LUA_STRLIBNAME, luaopen_string},
                {LUA_TABLIBNAME, luaopen_table},
                {LUA_MATHLIBNAME, luaopen_math},
                // {LUA_OSLIBNAME, luaopen_os},
                // {LUA_DBLIBNAME, luaopen_debug},
                // {"package", luaopen_package},  // removed in luau (sandboxing)
                // {"io", luaopen_io},            // removed in luau (sandboxing)
            });
            for (const auto& [name, function_ptr] : standard_library_functions) {
                lua_pushcfunction(state.get(), function_ptr);
                lua_pushstring(state.get(), name.c_str());
                lua_call(state.get(), 1, 0);
            }

            // load (don't run) the user-provided script as a callable function
            if (lua_load_cstringview(state.get(), source_code, chunk_name.c_str()) != 0) {
                OSC_ASSERT(lua_isstring(state.get(), 1) && "the top of the stack should contain an error message string");
                std::stringstream ss;
                ss << "lua_load: error: " << tostringview(state.get(), 1);
                return std::move(ss).str();  // ERROR: bad source code
            }

            // TODO: use `setfenv` to call the user-provided function with a specific
            // environment (e.g. imagine multiple environments sharing one VM)
            try {
                lua_call(state.get(), 0, 1);
            }
            catch (const std::exception& ex) {
                return std::string{ex.what()};  // ERROR: bad allocation, c function exception, etc.
            }

            OSC_ASSERT(lua_gettop(state.get()) == 1);
            std::string rv{tostringview(state.get(), -1)};
            lua_pop(state.get(), 1);
            return rv;
        }
    }

    struct TorusParameters final {
        float torus_radius = 1.0f;
        float tube_radius = 0.4f;

        friend bool operator==(const TorusParameters&, const TorusParameters&) = default;
    };

    class HelloTriangleScreen final : public Screen {
    public:
        HelloTriangleScreen()
        {
            // setup camera
            const Vec3 viewer_position = {3.0f, 0.0f, 0.0f};
            camera_.set_position(viewer_position);
            camera_.set_direction({-1.0f, 0.0f, 0.0f});

            // setup material
            material_.set_viewer_position(viewer_position);

            // setup ui code editor
            text_editor_.SetLanguageDefinition(TextEditor::LanguageDefinition::Lua());
            text_editor_.SetText("return string.reverse('adam', 'kewley')");
        }
    private:
        void impl_on_mount() override
        {
            ui::context::init(App::upd());
        }

        void impl_on_unmount() override
        {
            ui::context::shutdown();
        }

        bool impl_on_event(Event& e) override
        {
            return ui::context::on_event(e);
        }

        void impl_on_draw() override
        {
            App::upd().clear_screen();

            ui::context::on_start_new_frame(App::upd());

            // ensure target texture matches screen dimensions
            target_texture_.reformat({
                .dimensions = App::get().main_window_dimensions(),
                .anti_aliasing_level = App::get().anti_aliasing_level()
            });

            update_torus_if_params_changed();
            const auto seconds_since_startup = App::get().frame_delta_since_startup().count();
            const auto transform = identity<Transform>().with_rotation(angle_axis(Radians{seconds_since_startup}, Vec3{0.0f, 1.0f, 0.0f}));
            graphics::draw(mesh_, transform, material_, camera_);
            camera_.render_to(target_texture_);

            if (App::get().is_main_window_gamma_corrected()) {
                graphics::blit_to_screen(target_texture_, Rect{{}, App::get().main_window_dimensions()});
            }
            else {
                graphics::blit_to_screen(target_texture_, Rect{{}, App::get().main_window_dimensions()}, gamma_correcter_);
            }

            ui::begin_panel("window");
            ui::draw_text("source code");
            if (text_editor_.IsTextChanged()) {
                script_output_ = lua::compile_and_print("ui", text_editor_.GetText());
            }
            text_editor_.Render("editor", {0.0f, 10.0f*ui::get_text_line_height()});
            ui::draw_string_input("output", script_output_, ui::TextInputFlag::ReadOnly);
            ui::draw_float_slider("torus_radius", &edited_torus_parameters_.torus_radius, 0.0f, 5.0f);
            ui::draw_float_slider("tube_radius", &edited_torus_parameters_.tube_radius, 0.0f, 5.0f);
            ui::end_panel();

            ui::context::render();
        }

        void update_torus_if_params_changed()
        {
            if (torus_parameters_ == edited_torus_parameters_) {
                return;
            }
            mesh_ = TorusKnotGeometry{{
                .torus_radius = edited_torus_parameters_.torus_radius,
                .tube_radius = edited_torus_parameters_.tube_radius,
            }};
            torus_parameters_ = edited_torus_parameters_;
        }

        TorusParameters torus_parameters_;
        TorusParameters edited_torus_parameters_;
        TorusKnotGeometry mesh_;
        Color torus_color_ = Color::blue();
        MeshPhongMaterial material_{{
            .ambient_color = 0.2f * torus_color_,
            .diffuse_color = 0.5f * torus_color_,
            .specular_color = 0.5f * torus_color_,
        }};
        Material gamma_correcter_{Shader{c_gamma_correcting_vertex_shader_src, c_gamma_correcting_fragment_shader_src}};
        Camera camera_;
        RenderTexture target_texture_;
        TextEditor text_editor_;
        std::string script_output_ = lua::compile_and_print("ui-input", text_editor_.GetText());
    };
}


int main(int, char**)
{
#ifdef EMSCRIPTEN
    osc::App* app = new osc::App{};
    app->setup_main_loop<HelloTriangleScreen>();
    emscripten_set_main_loop_arg([](void* ptr) -> void
    {
        if (not static_cast<App*>(ptr)->do_main_loop_step()) {
            throw std::runtime_error{"exit"};
        }
    }, app, 0, EM_TRUE);
    return 0;
#else
    osc::App app;
    app.show<HelloTriangleScreen>();
    return 0;
#endif
}
