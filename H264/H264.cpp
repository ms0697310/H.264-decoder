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
		this->byteStream = vector<uint8_t>(std::istreambuf_iterator<char>(inputFile), {});
		inputFile.close();
		init();

		while (more_data_in_byte_stream() && isRunning) {


			byte_stream_nal_unit(0);
			//SDLRender();
		}
	}
private:
	void init() {
		//SDLInit();
		bitsInit();
	}

#pragma region NAL
	// B.1.1 Byte stream NAL unit syntax
	void byte_stream_nal_unit(int NumBytesInNALunit) {
		while (next_bits(24) != 0x000001 &&
			next_bits(32) != 0x00000001)
			read_bits(8);//leading_zero_8bits /* equal to 0x00 */ f(8)
		if (next_bits(24) != 0x000001)
			read_bits(8);//zero_byte /* equal to 0x00 */ f(8)
		read_bits(24); //start_code_prefix_one_3bytes /* equal to 0x000001 */ f(24)
		nal_unit(NumBytesInNALunit);
		while (more_data_in_byte_stream() &&
			next_bits(24) != 0x000001 &&
			next_bits(32) != 0x00000001)
			read_bits(8); //trailing_zero_8bits /* equal to 0x00 */ f(8)
	}
	//7.3.1 NAL unit syntax
	void nal_unit(int NumBytesInNALunit) {
		read_bits(1);//	forbidden_zero_bit All f(1)
		uint32_t nal_ref_idc = read_bits(2);//	nal_ref_idc All u(2)
		uint32_t nal_unit_type = read_bits(5);//	nal_unit_type All u(5)

		RBSP = vector<uint8_t>();//uint32_t	NumBytesInRBSP = 0;
		uint32_t	nalUnitHeaderBytes = 1;
		if (nal_unit_type == 14 || nal_unit_type == 20 ||
			nal_unit_type == 21) {
			bool svc_extension_flag = false;
			bool avc_3d_extension_flag = false;

			if (nal_unit_type != 21)
				svc_extension_flag = read_bits(1);// All u(1)
			else
				avc_3d_extension_flag = read_bits(1);// All u(1)
			if (svc_extension_flag) {
				nal_unit_header_svc_extension();// /* specified in Annex F */ All
				nalUnitHeaderBytes += 3;
			}
			else if (avc_3d_extension_flag) {
				nal_unit_header_3davc_extension();// /* specified in Annex I */
				nalUnitHeaderBytes += 2;
			}
			else {
				nal_unit_header_mvc_extension();///* specified in Annex G */ All
				nalUnitHeaderBytes += 3;
			}
		}
		for (size_t i = nalUnitHeaderBytes; i < NumBytesInNALunit; i++) {
			if (i + 2 < NumBytesInNALunit && next_bits(24) == 0x000003) {
				RBSP.push_back(read_bits(8));//rbsp_byte[NumBytesInRBSP++]  All b(8)
				RBSP.push_back(read_bits(8));//rbsp_byte[NumBytesInRBSP++]  All b(8)
				i += 2;
				read_bits(8);// emulation_prevention_three_byte /* equal to 0x03 */ All f(8)
			}
			else
				RBSP.push_back(read_bits(8)); //rbsp_byte[NumBytesInRBSP++] All b(8)
		}
	}
	//F.7.3.1.1 NAL unit header SVC extension syntax
	void nal_unit_header_svc_extension() {
		read_bits(1);//idr_flag All u(1)
		read_bits(6);//priority_id All u(6)
		read_bits(1);//no_inter_layer_pred_flag All u(1)
		read_bits(3);//dependency_id All u(3)
		read_bits(4);//quality_id All u(4)
		read_bits(3);//temporal_id All u(3)
		read_bits(1);//use_ref_base_pic_flag All u(1)
		read_bits(1);//discardable_flag All u(1)
		read_bits(1);//output_flag All u(1)
		read_bits(2);//reserved_three_2bits All u(2)
	}
	//I.7.3.1.1 NAL unit header 3D-AVC extension syntax
	void nal_unit_header_3davc_extension() {
		read_bits(8);//view_idx All u(8)
		read_bits(1);//depth_flag All u(1)
		read_bits(1);//non_idr_flag All u(1)
		read_bits(3);//temporal_id All u(3)
		read_bits(1);//anchor_pic_flag All u(1)
		read_bits(1);//inter_view_flag All u(1)
	}
	//G.7.3.1.1 NAL unit header MVC extension syntax
	void nal_unit_header_mvc_extension() {
		read_bits(1);//non_idr_flag All u(1)
		read_bits(6);//priority_id All u(6)
		read_bits(10);//view_id All u(10)
		read_bits(3);//temporal_id All u(3)
		read_bits(1);//anchor_pic_flag All u(1)
		read_bits(1);//inter_view_flag All u(1)
		read_bits(1);//reserved_one_bit All u(1)
	}
#pragma endregion

#pragma region bits
	vector<uint8_t> byteStream;
	size_t index;
	uint32_t  bits = 0;
	size_t bitsIndex = 0;
	vector<uint8_t> RBSP;
	bool RBSPMode = false;
	void bitsInit() {
		index = 0;
		bitsIndex = 32;
		bits = 0;
		while (bitsIndex >= 8) {
			bits = (bits << 8) | byteStream[index++];
			bitsIndex -= 8;
		}
		RBSP = vector<uint8_t>();
	}

	// 7.2 Specification of syntax functions, categories, and descriptors
	bool byte_aligned() {
		return bitsIndex % 8 == 0;
	}
	bool more_data_in_byte_stream() {
		return (index < byteStream.size());
	}
	bool more_rbsp_data() {
		if (RBSP.size() == 0) return false;
		return false;
	}
	bool more_rbsp_trailing_data() {
		return RBSP.size() > 0;
	}
	uint32_t next_bits(size_t n) {
		if (n == 0) return 0;
		return bits >> (32 - n);
	}
	// f(n)  u(n) b(8) p.68
	uint32_t read_bits(size_t n) {
		if (n == 0) return 0;
		uint32_t num = next_bits(n);
		bits <<= n;
		bitsIndex += n;
		while (bitsIndex >= 8 && more_data_in_byte_stream()) {
			bits = (bits << 8) | byteStream[index++];
			bitsIndex -= 8;
		}
		return num;
	}
	// 7.3.2.11 RBSP trailing bits syntax
	void rbsp_trailing_bits() {
		read_bits(1);//rbsp_stop_one_bit /* equal to 1 */ All f(1)
		while (!byte_aligned())
			read_bits(1);//rbsp_alignment_zero_bit /* equal to 0 */ All f(1)
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
	void SDLRender() {

		//處理事件
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT) {
				isRunning = false;
			}
		}
		uint16_t a = read_bits(8);
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
#pragma endregion


};


int main(int argc, char* argv[]) {


	H264Decoder video("video/BadApple.mp4");

	return 0;
}
