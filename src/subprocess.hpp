#pragma once
#include <Windows.h>
#include <string>

namespace subprocess {

    struct PipeHandle {
        HANDLE handle = nullptr;

        void set_inherit(bool value) {
            if (handle) {
                SetHandleInformation(handle, HANDLE_FLAG_INHERIT, value * HANDLE_FLAG_INHERIT);
            }
        }

        void close() {
            if (handle) {
                CloseHandle(handle);
                handle = nullptr;
            }
        }
    };

    struct PipePair {
        PipeHandle m_read;
        PipeHandle m_write;

        static PipePair create(bool inheritable) {
            SECURITY_ATTRIBUTES security = {};
            security.nLength = sizeof(security);
            security.bInheritHandle = inheritable;
            HANDLE read, write;
            CreatePipe(&read, &write, &security, 0);
            return { { read }, { write } };
        }

        void write(const void* const data, size_t size) {
            WriteFile(m_write.handle, data, size, nullptr, nullptr);
        }

        std::string read() {
            std::string output;
            char buffer[4096];
            DWORD bytes_read = 0;
            while (ReadFile(m_read.handle, buffer, sizeof(buffer), &bytes_read, nullptr) && bytes_read > 0) {
                output.append(buffer, bytes_read);
            }
            return output;
        }

        void close() {
            m_read.close();
            m_write.close();
        }
    };

    class Popen {
    public:
        PipePair m_stdin;
        PipePair m_stdout;
        PROCESS_INFORMATION m_proc_info{};
    public:
        Popen() {}
        Popen(const std::wstring& command, bool redirect_output = false, bool redirect_input = false, bool show_window = false) {
            STARTUPINFOW start_info = {};

            start_info.cb = sizeof(start_info);
            start_info.dwFlags |= STARTF_USESTDHANDLES;

            if (redirect_output) {
                m_stdout = PipePair::create(true);
                start_info.hStdOutput = m_stdout.m_write.handle;
                start_info.hStdError = m_stdout.m_write.handle;
                m_stdout.m_read.set_inherit(false);
            } else {
                start_info.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
                start_info.hStdError = GetStdHandle(STD_ERROR_HANDLE);
            }

            if (redirect_input) {
                m_stdin = PipePair::create(true);
                start_info.hStdInput = m_stdin.m_read.handle;
                m_stdin.m_write.set_inherit(false);
            } else {
                start_info.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
            }

            CreateProcessW(nullptr, (wchar_t*) command.data(), nullptr, nullptr, true, 0 | (show_window ? 0 : CREATE_NO_WINDOW), nullptr, nullptr, &start_info, &m_proc_info);

            if (redirect_input) {
                m_stdin.m_read.close();
            }
            if (redirect_output) {
                m_stdout.m_write.close();
            }
        }

        int wait() {
            if (m_proc_info.hProcess == nullptr) {
                return -1;
            }

            WaitForSingleObject(m_proc_info.hProcess, INFINITE);
            DWORD exit_code;
            GetExitCodeProcess(m_proc_info.hProcess, &exit_code);
            return exit_code;
        }

        int close(bool should_wait = true) {
            int exit_code = 0;
            m_stdin.close();
            if (should_wait) exit_code = wait();
            CloseHandle(m_proc_info.hProcess);
            CloseHandle(m_proc_info.hThread);
            return exit_code;
        }

        bool is_running() const {
            if (m_proc_info.hProcess == nullptr) {
                return false;
            }
            
            DWORD result = WaitForSingleObject(m_proc_info.hProcess, 0);
            return (result == WAIT_TIMEOUT);
        }
    };
}