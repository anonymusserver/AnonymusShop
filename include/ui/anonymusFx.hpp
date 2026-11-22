#pragma once
// Anonymus Shop: efectos con movimiento (lluvia de código, brillos que laten y "glitch" del logo).
#include <pu/Plutonium>
#include <SDL2/SDL_image.h>
#include <chrono>
#include <cmath>
#include <random>
#include <string>
#include <vector>

namespace inst::ui::fx {

    inline double NowSeconds() {
        using clock = std::chrono::steady_clock;
        static const auto start = clock::now();
        return std::chrono::duration<double>(clock::now() - start).count();
    }

    // Lluvia de caracteres tipo Matrix. Se dibuja detrás de todo lo demás.
    class CodeRain : public pu::ui::elm::Element {
        public:
            CodeRain(s32 x, s32 y, s32 w, s32 h, int columnStep, int maxAlpha, double speedScale = 1.0)
                : x(x), y(y), w(w), h(h), step(columnStep), maxAlpha(maxAlpha), speedScale(speedScale), rng(1337) {}
            PU_SMART_CTOR(CodeRain)
            ~CodeRain() override {
                for (auto t : this->trailGlyphs) pu::ui::render::DeleteTexture(t);
                for (auto t : this->headGlyphs) pu::ui::render::DeleteTexture(t);
            }
            s32 GetX() override { return this->x; }
            s32 GetY() override { return this->y; }
            s32 GetWidth() override { return this->w; }
            s32 GetHeight() override { return this->h; }
            void SetMaxAlpha(int alpha) { this->maxAlpha = alpha; }
            void OnInput(u64, u64, u64, pu::ui::Touch) override {}

            void OnRender(pu::ui::render::Renderer::Ref& drawer, s32 rx, s32 ry) override {
                if (!this->ready) this->Setup();
                if (this->trailGlyphs.empty()) return;
                const double now = NowSeconds();
                double dt = this->lastTime < 0 ? 0.016 : now - this->lastTime;
                this->lastTime = now;
                if (dt > 0.1) dt = 0.1;
                std::uniform_real_distribution<double> unit(0.0, 1.0);
                const int glyphs = static_cast<int>(this->trailGlyphs.size());
                for (auto& c : this->columns) {
                    c.head += c.speed * dt * this->speedScale;
                    if (c.head - c.length * kRow > this->h) this->Reset(c, false);
                    if (unit(this->rng) < 0.06) c.chars[static_cast<std::size_t>(unit(this->rng) * c.chars.size()) % c.chars.size()] = static_cast<int>(unit(this->rng) * glyphs) % glyphs;
                    const int headRow = static_cast<int>(c.head / kRow);
                    for (int k = 0; k < c.length; ++k) {
                        const int row = headRow - k;
                        const int py = row * kRow;
                        if (py < -kRow || py > this->h) continue;
                        const int glyph = c.chars[static_cast<std::size_t>(row & 63) % c.chars.size()];
                        pu::ui::render::NativeTextureRenderOptions opts = pu::ui::render::NativeTextureRenderOptions::Default;
                        const double fade = 1.0 - static_cast<double>(k) / c.length;
                        opts.AlphaMod = std::max(0, std::min(255, static_cast<int>(this->maxAlpha * c.bright * fade * fade)));
                        if (opts.AlphaMod < 6) continue;
                        auto tex = k == 0 ? this->headGlyphs[glyph] : this->trailGlyphs[glyph];
                        if (k == 0) opts.AlphaMod = std::min(255, static_cast<int>(this->maxAlpha * 1.3));
                        drawer->RenderTexture(tex, rx + c.x, ry + py, opts);
                    }
                }
            }

        private:
            static constexpr int kRow = 20;
            struct Column { int x; double head; double speed; int length; double bright; std::vector<int> chars; };

            void Setup() {
                this->ready = true;
                const std::string set = "0123456789ABCDEFXZ$#@%&*+=<>/\\|{}[]:;0101";
                auto font = pu::ui::render::LoadDefaultFont(18);
                auto fallback = pu::ui::render::LoadSharedFont(pu::ui::render::SharedFont::NintendoExtended, 18);
                for (char ch : set) {
                    const std::string s(1, ch);
                    auto trail = pu::ui::render::RenderText(font, fallback, s, pu::ui::Color(0, 255, 102, 255));
                    auto head = pu::ui::render::RenderText(font, fallback, s, pu::ui::Color(210, 255, 225, 255));
                    if (!trail || !head) continue;
                    this->trailGlyphs.push_back(trail);
                    this->headGlyphs.push_back(head);
                }
                for (int cx = 4; cx < this->w; cx += this->step) {
                    Column c;
                    c.x = cx;
                    this->columns.push_back(c);
                    this->Reset(this->columns.back(), true);
                }
            }

            void Reset(Column& c, bool initial) {
                std::uniform_real_distribution<double> unit(0.0, 1.0);
                c.length = 6 + static_cast<int>(unit(this->rng) * 18);
                c.speed = 60.0 + unit(this->rng) * 170.0;
                c.bright = 0.35 + unit(this->rng) * 0.65;
                c.head = initial ? unit(this->rng) * (this->h + 400) - 200 : -unit(this->rng) * 300;
                c.chars.resize(64);
                const int glyphs = std::max<int>(1, static_cast<int>(this->trailGlyphs.size()));
                for (auto& g : c.chars) g = static_cast<int>(unit(this->rng) * glyphs) % glyphs;
            }

            s32 x, y, w, h;
            int step;
            int maxAlpha;
            double speedScale;
            bool ready = false;
            double lastTime = -1;
            std::mt19937 rng;
            std::vector<Column> columns;
            std::vector<pu::ui::render::NativeTexture> trailGlyphs;
            std::vector<pu::ui::render::NativeTexture> headGlyphs;
    };

    // Imagen que late (sube y baja de brillo) o que aparece a golpes con temblor ("glitch").
    class PulseImage : public pu::ui::elm::Element {
        public:
            enum class Mode { Pulse, Glitch };
            PulseImage(s32 x, s32 y, const std::string& path, Mode mode, double period, int minAlpha, int maxAlpha)
                : x(x), y(y), path(path), mode(mode), period(period), minAlpha(minAlpha), maxAlpha(maxAlpha), rng(4242) {}
            PU_SMART_CTOR(PulseImage)
            ~PulseImage() override { pu::ui::render::DeleteTexture(this->texture); }
            s32 GetX() override { return this->x; }
            s32 GetY() override { return this->y; }
            s32 GetWidth() override { return this->w; }
            s32 GetHeight() override { return this->h; }
            void OnInput(u64, u64, u64, pu::ui::Touch) override {}

            void OnRender(pu::ui::render::Renderer::Ref& drawer, s32 rx, s32 ry) override {
                if (!this->loaded) {
                    this->loaded = true;
                    SDL_Surface* surface = IMG_Load(this->path.c_str());
                    if (surface) {
                        this->texture = SDL_CreateTextureFromSurface(pu::ui::render::GetMainRenderer(), surface);
                        this->w = surface->w;
                        this->h = surface->h;
                        SDL_FreeSurface(surface);
                    }
                }
                if (!this->texture) return;
                const double t = NowSeconds();
                pu::ui::render::NativeTextureRenderOptions opts = pu::ui::render::NativeTextureRenderOptions::Default;
                if (this->mode == Mode::Pulse) {
                    const double s = 0.5 + 0.5 * std::sin(t * 2.0 * 3.14159265358979 / this->period);
                    opts.AlphaMod = this->minAlpha + static_cast<int>((this->maxAlpha - this->minAlpha) * s);
                    drawer->RenderTexture(this->texture, rx, ry, opts);
                    return;
                }
                // Glitch: cada 'period' segundos, unos pocos fotogramas con desplazamiento aleatorio.
                const double phase = std::fmod(t, this->period);
                if (phase > 0.22) return;
                std::uniform_int_distribution<int> jitter(-7, 7);
                opts.AlphaMod = this->maxAlpha;
                drawer->RenderTexture(this->texture, rx + jitter(this->rng), ry + jitter(this->rng) / 3, opts);
            }

        private:
            s32 x, y;
            s32 w = 0, h = 0;
            std::string path;
            Mode mode;
            double period;
            int minAlpha, maxAlpha;
            bool loaded = false;
            std::mt19937 rng;
            pu::ui::render::NativeTexture texture = nullptr;
    };

    // Color del borde de selección que late.
    inline pu::ui::Color PulsingAccent(int minAlpha, int maxAlpha, double period = 1.4) {
        const double s = 0.5 + 0.5 * std::sin(NowSeconds() * 2.0 * 3.14159265358979 / period);
        return pu::ui::Color(0, 255, 102, static_cast<u8>(minAlpha + (maxAlpha - minAlpha) * s));
    }

}
