module;

#include <vulkan/vulkan_core.h>
#include <GLFW/glfw3.h>

export module vkutil:window;

import std;
import glm;
import vulkan_hpp;

export namespace vkutil {
    struct GlfwWindow {
        GLFWwindow *window;

        GlfwWindow(int width, int height, const char *title)
            : window { glfwCreateWindow(width, height, title, nullptr, nullptr) } {
            glfwSetWindowUserPointer(window, this);

            glfwSetWindowSizeCallback(window, [](GLFWwindow *window, int width, int height) {
                static_cast<GlfwWindow*>(glfwGetWindowUserPointer(window))
                    ->onSizeCallback({ width, height });
            });
            glfwSetFramebufferSizeCallback(window, [](GLFWwindow *window, int width, int height) {
                static_cast<GlfwWindow*>(glfwGetWindowUserPointer(window))
                    ->onFramebufferSizeCallback({ width, height });
            });
            glfwSetWindowContentScaleCallback(window, [](GLFWwindow *window, float xscale, float yscale) {
                static_cast<GlfwWindow*>(glfwGetWindowUserPointer(window))
                    ->onContentScaleCallback({ xscale, yscale });
            });
            glfwSetKeyCallback(window, [](GLFWwindow *window, int key, int scancode, int action, int mods) {
                static_cast<GlfwWindow*>(glfwGetWindowUserPointer(window))
                    ->onKeyCallback(key, scancode, action, mods);
            });
            glfwSetCharCallback(window, [](GLFWwindow *window, unsigned int codepoint) {
                static_cast<GlfwWindow*>(glfwGetWindowUserPointer(window))
                    ->onCharCallback(codepoint);
            });
            glfwSetCursorPosCallback(window, [](GLFWwindow *window, double xpos, double ypos) {
                static_cast<GlfwWindow*>(glfwGetWindowUserPointer(window))
                    ->onCursorPosCallback({ xpos, ypos });
            });
            glfwSetCursorEnterCallback(window, [](GLFWwindow *window, int entered) {
                static_cast<GlfwWindow*>(glfwGetWindowUserPointer(window))
                    ->onCursorEnterCallback(entered);
            });
            glfwSetMouseButtonCallback(window, [](GLFWwindow *window, int button, int action, int mods) {
                static_cast<GlfwWindow*>(glfwGetWindowUserPointer(window))
                    ->onMouseButtonCallback(button, action, mods);
            });
            glfwSetScrollCallback(window, [](GLFWwindow *window, double xoffset, double yoffset) {
                static_cast<GlfwWindow*>(glfwGetWindowUserPointer(window))
                    ->onScrollCallback({ xoffset, yoffset });
            });
            glfwSetDropCallback(window, [](GLFWwindow *window, int count, const char **paths) {
                static_cast<GlfwWindow*>(glfwGetWindowUserPointer(window))
                    ->onDropCallback({ paths, static_cast<std::size_t>(count) });
            });
        }
        GlfwWindow(const GlfwWindow&) = delete;
        GlfwWindow(GlfwWindow &&src) noexcept
            : window { std::exchange(src.window, nullptr) } {

        }
        auto operator=(const GlfwWindow&) -> GlfwWindow& = delete;
        auto operator=(GlfwWindow &&src) noexcept -> GlfwWindow& {
            if (window) {
                glfwDestroyWindow(window);
            }

            window = std::exchange(src.window, nullptr);
            return *this;
        }

        virtual ~GlfwWindow() {
            if (window) {
                glfwDestroyWindow(window);
            }
        }

        [[nodiscard]] auto createSurface(vk::Instance instance) const -> vk::SurfaceKHR {
            vk::SurfaceKHR surface;
            if (glfwCreateWindowSurface(instance, window, nullptr, reinterpret_cast<VkSurfaceKHR*>(&surface)) != VK_SUCCESS) {
                throw std::runtime_error { "Failed to create window surface." };
            }
            return surface;
        }

        [[nodiscard]] auto shouldClose() const -> bool {
            return glfwWindowShouldClose(window);
        }

        [[nodiscard]] auto getSize() const -> glm::ivec2 {
            glm::ivec2 size;
            glfwGetWindowSize(window, &size.x, &size.y);
            return size;
        }

        [[nodiscard]] auto getFramebufferSize() const -> glm::ivec2 {
            glm::ivec2 size;
            glfwGetFramebufferSize(window, &size.x, &size.y);
            return size;
        }

        [[nodiscard]] auto getFramebufferAspectRatio() const -> float {
            const glm::ivec2 size = getFramebufferSize();
            return static_cast<float>(size.x) / static_cast<float>(size.y);
        }

        [[nodiscard]] auto getCursorPos() const -> glm::dvec2 {
            glm::dvec2 pos;
            glfwGetCursorPos(window, &pos.x, &pos.y);
            return pos;
        }

        [[nodiscard]] auto getFramebufferCursorPos() const -> glm::dvec2 {
            return glm::dvec2 { getContentScale() } * getCursorPos();
        }

        [[nodiscard]] auto getContentScale() const -> glm::vec2 {
            glm::vec2 scale;
            glfwGetWindowContentScale(window, &scale.x, &scale.y);
            return scale;
        }

        void setTitle(const char *title) const {
            glfwSetWindowTitle(window, title);
        }

        virtual void onSizeCallback(glm::ivec2 size) { }
        virtual void onFramebufferSizeCallback(glm::ivec2 size) { }
        virtual void onContentScaleCallback(glm::vec2 scale) { }
        virtual void onKeyCallback(int key, int scancode, int action, int mods) { }
        virtual void onCharCallback(unsigned int codepoint) { }
        virtual void onCursorPosCallback(glm::dvec2 position) { }
        virtual void onCursorEnterCallback(int entered) { }
        virtual void onMouseButtonCallback(int button, int action, int mods) { }
        virtual void onScrollCallback(glm::dvec2 offset) { }
        virtual void onDropCallback(std::span<const char*> paths) { }

        virtual void beforeLoop() { }
        virtual void onLoop(float timeDelta) { };
        virtual void afterLoop() { }

        void run() {
            beforeLoop();

            float lastTime = glfwGetTime();
            while (!glfwWindowShouldClose(window)) {
                glfwPollEvents();

                const float timeDelta = glfwGetTime() - lastTime;
                lastTime += timeDelta;

                onLoop(timeDelta);
            }

            afterLoop();
        }

        [[nodiscard]] static auto getInstanceExtensions() -> std::vector<const char*> {
            std::uint32_t count;
            const char **extensions = glfwGetRequiredInstanceExtensions(&count);
            return { extensions, extensions + count };
        }

        static void pollEvents() {
            glfwPollEvents();
        }
    };
}