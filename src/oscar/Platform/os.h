#pragma once

#include <oscar/Platform/LogLevel.h>
#include <oscar/Utils/CStringView.h>

#include <ctime>
#include <filesystem>
#include <initializer_list>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// os: where all the icky OS/distro/filesystem-specific stuff is hidden
namespace osc
{
    // returns current system time as a calendar time
    std::tm system_calendar_time();

    // returns a `std::tm` populated 'as-if' by calling `std::gmtime(&t)`, but in
    // an implementation-defined threadsafe way
    std::tm gmtime_threadsafe(std::time_t);

    // returns a `std::string` describing the current value of `errno`, but in
    // an implementation-defined threadsafe way
    std::string errno_to_string_threadsafe();

    // returns a `std::string` describing the given error number (errnum), but in
    // an implementation-defined threadsafe way
    std::string strerror_threadsafe(int errnum);

    // returns the full path to the currently-executing application
    //
    // care: can be slow: downstream callers should cache it
    std::filesystem::path current_executable_directory();

    // returns the full path to the user's data directory
    std::filesystem::path user_data_directory(
        CStringView organization_name,
        CStringView application_name
    );

    // writes a backtrace for the calling thread's stack to the process-wide
    // log at the specified logging level
    void write_this_thread_backtrace_to_log(LogLevel);

    // installs a signal handler for crashes (SIGABRT/SIGSEGV, etc.) that will
    // print a thread backtrace to the process-wide log, followed by trying to
    // write a crash report as `CrashReport_DATE.txt` to `crash_dump_directory`
    void enable_crash_signal_backtrace_handler(const std::filesystem::path& crash_dump_directory);

    // tries to open the specified filepath in the OS's default application for opening
    // a path (usually, based on its extension)
    //
    // - this function returns immediately: the application is opened in a separate
    //   window
    //
    // - how, or what, the OS does is implementation-defined. E.g. on Windows, this
    //   opens the path by searching the file's extension against a list of default
    //   applications or, if Windows detects it's a URL, it will open the URL in
    //   the user's default web browser, etc.
    void open_file_in_os_default_application(const std::filesystem::path&);

    // tries to open the specified URL in the OS's default browser
    void open_url_in_os_default_web_browser(CStringView);

    // returns `true` if `content` was sucessfully copied to the user's clipboard
    bool set_clipboard_text(CStringView content);

    // sets an environment variable's value process-wide
    //
    // if `overwrite` is `true`, then it overwrites any previous value; otherwise,
    // it will only set the environment variable if no environment variable with
    // `name` exists
    void set_environment_variable(CStringView name, CStringView value, bool overwrite);

    // set the current process's HighDPI mode
    //
    // - must be called before a window is created
    // - OS-dependent: some OSes handle this as a window-creation argument, rather
    //   than as a process-wide function call
    void enable_highdpi_mode_for_this_process();

    // synchronously prompt a user to select a single file using the OS's native file
    // browser
    //
    // - `file_extensions` can be:
    //   - empty, meaning "don't filter by extension"
    //   - nonempty, meaning "filter by these extensions"
    //
    // - `initial_directory_to_show` can be:
    //   - `std::nullopt`, meaning "use a system-defined default"
    //   - a path to a directory to initially show to the user when the prompt opens
    //
    // returns `std::nullopt` if the user doesn't select a file
    std::optional<std::filesystem::path> prompt_user_to_select_file(
        std::span<const std::string_view> file_extensions = {},
        std::optional<std::filesystem::path> initial_directory_to_show = std::nullopt
    );
    inline std::optional<std::filesystem::path> prompt_user_to_select_file(
        std::initializer_list<std::string_view> file_extensions = {},
        std::optional<std::filesystem::path> initial_directory_to_show = std::nullopt)
    {
        return prompt_user_to_select_file(
            std::span<const std::string_view>{file_extensions},
            std::move(initial_directory_to_show)
        );
    }

    // synchronously prompt a user to select files ending with the supplied extensions (e.g. "txt, csv, tsv")
    //
    // - `file_extensions` can be:
    //   - empty, meaning "don't filter by extension"
    //   - nonempty, meaning "filter by these extensions"
    //
    // - `initial_directory_to_show` can be:
    //   - `std::nullopt`, meaning "use a system-defined default"
    //   - a path to a directory to initially show to the user when the prompt opens
    //
    // returns an empty vector if the user doesn't select any files
    std::vector<std::filesystem::path> prompt_user_to_select_files(
        std::span<const std::string_view> file_extensions = {},
        std::optional<std::filesystem::path> initial_directory_to_show = std::nullopt
    );
    inline std::vector<std::filesystem::path> prompt_user_to_select_files(
        std::initializer_list<std::string_view> file_extensions = {},
        std::optional<std::filesystem::path> initial_directory_to_show = std::nullopt)
    {
        return prompt_user_to_select_files(
            std::span<const std::string_view>{file_extensions},
            std::move(initial_directory_to_show)
        );
    }

    // synchronously prompt a user to select a file location for where to save a file
    //
    // - `maybeOneExtension` can be:
    //   - std::nullopt, meaning "don't filter by extension"
    //   - or a single extension (e.g. "blend")
    //   - (you can't use multiple extensions with this method)
    //
    // - `maybeInitialDirectoryToOpen` can be:
    //   - std::nullopt, meaning "use a system-defined default"
    //   - a directory to initially show to the user when the prompt opens
    //
    // - if the user manually types a filename without an extension (e.g. "model"), the implementation will add `extension`
    //   (if not std::nullopt) to the end of the user's string. It detects a lack of extension by searching the end of the user
    //   -supplied string for the given extension (if supplied)
    //
    // returns std::nullopt if the user doesn't select a file; otherwise, returns the user-selected save location--including the extension--if
    // the user selects a location
    std::optional<std::filesystem::path> PromptUserForFileSaveLocationAndAddExtensionIfNecessary(
        std::optional<CStringView> maybeExtension = std::nullopt,
        std::optional<CStringView> maybeInitialDirectoryToOpen = std::nullopt
    );
}
