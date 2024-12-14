#include <SDL.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <bitset>
#include <string>
#include <cmath>
//#define DEBUG
using namespace std;

class H264Decoder {
public:
    H264Decoder(string fileName) {
        ifstream inputFile(fileName, ios::binary);
        if (!inputFile) {
            cerr << "無法打開檔案！" << endl;
        }
        this->_buffer = vector<uint8_t>(std::istreambuf_iterator<char>(inputFile), {});
        inputFile.close();
        index = 0;
        init();

        while (index < _buffer.size() && isRunning) {
            index++;
            //cout<<hex<< bitset<8>(getByte()).to_ullong()<<" ";

            // 處理事件
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) {
                    isRunning = false;
                }
            }
            uint16_t a = bitset<8>(getByte()).to_ullong();
            // 設置像素顏色為紅色
            SDL_SetRenderDrawColor(renderer, a, a, a, 255);
            // 繪製單個像素
            for (int x = 0; x < 800; x++) {
                for (int y = 0; y < 600; y++) {
                    if ((x) % 10 == 0) { // 例：只繪製部分像素
                        SDL_RenderDrawPoint(renderer, x, y);
                    }
                }
            }
            // 顯示渲染結果
            SDL_RenderPresent(renderer);
        }
    }
private:

#pragma region bits
    vector<uint8_t> _buffer;
    size_t index;
    uint16_t get2Bytes() {
        return bitset<16>(((uint16_t)_buffer[index]) << 8 | _buffer[index + 1]).to_ullong();
    }
    uint16_t getByte() {
        return bitset<8>(_buffer[index]).to_ulong();
    }
    uint16_t getLeft4Bits() {
        return bitset<4>(_buffer[index] >> 4).to_ulong();
    }
    uint16_t getRight4Bits() {
        return bitset<4>(_buffer[index] & 0x0f).to_ulong();
    }
#pragma endregion

#pragma region SDL
    bool isRunning = true;
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Event event;
    int SDLInit() {
        // 初始化 SDL
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
            return -1;
        }

        // 創建窗口
        window = SDL_CreateWindow(
            "SDL Tutorial",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            800, 600,
            SDL_WINDOW_SHOWN
        );

        if (!window) {
            std::cerr << "Window could not be created! SDL_Error: " << SDL_GetError() << std::endl;
            SDL_Quit();
            return -1;
        }

        // 創建渲染器
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
        if (!renderer) {
            std::cerr << "Renderer could not be created! SDL_Error: " << SDL_GetError() << std::endl;
            SDL_DestroyWindow(window);
            SDL_Quit();
            return -1;
        }

        // 清屏
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
    }
    int SDLDestroy() {
        // 清理資源
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 0;
    }
#pragma endregion

    void init() {
        SDLInit();
    }
};	


int main(int argc, char* argv[]) {


    H264Decoder video("video/BadApple.mp4");

    return 0;
}
