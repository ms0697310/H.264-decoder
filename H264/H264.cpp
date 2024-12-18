#include <SDL.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <bitset>
#include <string>
#include <cmath>
#include "assert.h" 
#include <iomanip>
//#define DEBUG
using namespace std;
#define ChromaArrayType (separate_colour_plane_flag == 0) ? chroma_format_idc : 0
#define SliceGroupChangeRate slice_group_change_rate_minus1 + 1
#define SubWidthC (chroma_format_idc == 1||chroma_format_idc == 2) ? 2: (chroma_format_idc == 3? 1 : 0 )
#define SubHeightC ((chroma_format_idc == 2||chroma_format_idc == 3)&& separate_colour_plane_flag == 0) ? 1: (chroma_format_idc == 1? 2 : 0 )
#define MbWidthC (chroma_format_idc == 0 || separate_colour_plane_flag == 1) ? 0 : (16 / SubWidthC) // 6-1
#define MbHeightC (chroma_format_idc == 0 || separate_colour_plane_flag == 1) ? 0 : (16 / SubHeightC) // 6-2
#define BitDepthY 8 + bit_depth_luma_minus8  // 7-3
#define QpBdOffsetY 6 * bit_depth_luma_minus8 // 7-4
#define BitDepthC  8 + bit_depth_chroma_minus8 // 7-5
#define QpBdOffsetC 6 * bit_depth_chroma_minus8 // 7-6
#define RawMbBits  256 * BitDepthY + 2 * MbWidthC * MbHeightC * BitDepthC // 7-7
#define PicWidthInMbs pic_width_in_mbs_minus1 + 1 // 7-13
#define PicWidthInSamplesL PicWidthInMbs * 16 // 7-14
#define PicWidthInSamplesC PicWidthInMbs * MbWidthC  // 7-15
#define PicHeightInMapUnits pic_height_in_map_units_minus1 + 1 // 7-16
#define PicSizeInMapUnits  PicWidthInMbs * PicHeightInMapUnits // 7-17
#define FrameHeightInMbs ( 2 - frame_mbs_only_flag ) * PicHeightInMapUnits  // 7-18
#define IdrPicFlag  ( ( nal_unit_type == 5 ) ? 1 : 0 )
#define MbaffFrameFlag ( mb_adaptive_frame_field_flag && !field_pic_flag ) 
#define PicHeightInMbs  FrameHeightInMbs / ( 1 + field_pic_flag ) 
#define PicHeightInSamplesL  PicHeightInMbs * 16
#define PicHeightInSamplesC  PicHeightInMbs * MbHeightC 
#define PicSizeInMbs  PicWidthInMbs * PicHeightInMbs
#define MapUnitsInSliceGroup0 min( slice_group_change_cycle * SliceGroupChangeRate, PicSizeInMapUnits)// 7-36
// 8-24 8-25 8-26
#define MbToSliceGroupMap(i) (frame_mbs_only_flag == 1 || field_pic_flag == 1)\
? mapUnitToSliceGroupMap[i] : MbaffFrameFlag == 1 \
? mapUnitToSliceGroupMap[ i / 2 ] \
: mapUnitToSliceGroupMap[ ( i / ( 2 * PicWidthInMbs ) ) * PicWidthInMbs + (i % PicWidthInMbs)]
class H264Decoder {
public:
	H264Decoder(string fileName) {
		ifstream inputFile(fileName, ios::binary);
		if (!inputFile) {
			cerr << "無法打開檔案！" << endl;
		}
		this->fileByteStream = vector<uint8_t>(std::istreambuf_iterator<char>(inputFile), {});
		this->byteStream = &fileByteStream;
		inputFile.close();
		init();
		while (more_data_in_byte_stream()) {
			byte_stream_nal_unit();
		}
		cout << "END" << endl;
		//SDLRender();
	}
private:
	void init() {
		//SDLInit();
		bitsInit();
	}

	enum SliceType { P = 0, B, I, SP, SI };
	enum MBType_I {
		// I
		I_NxN = 0,
		I_16x16_0_0_0,
		I_16x16_1_0_0,
		I_16x16_2_0_0,
		I_16x16_3_0_0,
		I_16x16_0_1_0,
		I_16x16_1_1_0,
		I_16x16_2_1_0,
		I_16x16_3_1_0,
		I_16x16_0_2_0,
		I_16x16_1_2_0,
		I_16x16_2_2_0,
		I_16x16_3_2_0,
		I_16x16_0_0_1,
		I_16x16_1_0_1,
		I_16x16_2_0_1,
		I_16x16_3_0_1,
		I_16x16_0_1_1,
		I_16x16_1_1_1,
		I_16x16_2_1_1,
		I_16x16_3_1_1,
		I_16x16_0_2_1,
		I_16x16_1_2_1,
		I_16x16_2_2_1,
		I_16x16_3_2_1,
		I_PCM,
	};
	enum MBType_P {
		// P and SP
		P_L0_16x16 = 0,
		P_L0_L0_16x8,
		P_L0_L0_8x16,
		P_8x8,
		P_8x8ref0,
		P_Skip,
	};
	enum MBType_B {
		// B
		B_Direct_16x16 = 0,
		B_L0_16x16,
		B_L1_16x16,
		B_Bi_16x16,
		B_L0_L0_16x8,
		B_L0_L0_8x16,
		B_L1_L1_16x8,
		B_L1_L1_8x16,
		B_L0_L1_16x8,
		B_L0_L1_8x16,
		B_L1_L0_16x8,
		B_L1_L0_8x16,
		B_L0_Bi_16x8,
		B_L0_Bi_8x16,
		B_L1_Bi_16x8,
		B_L1_Bi_8x16,
		B_Bi_L0_16x8,
		B_Bi_L0_8x16,
		B_Bi_L1_16x8,
		B_Bi_L1_8x16,
		B_Bi_Bi_16x8,
		B_Bi_Bi_8x16,
		B_8x8,
		B_Skip
	};
	enum MbPartPredModeType {
		na,
		// I
		Intra_4x4,
		Intra_8x8,
		Intra_16x16,
		// B, P and SP
		Direct,
		Pred_L0,
		Pred_L1,
		BiPred,
	};
	bool sliceTypeCheck(SliceType type) {
		return slice_type % 5 == type;
	}
#pragma region NAL
	int cc = 0;
	// B.1.1 Byte stream NAL unit syntax
	void byte_stream_nal_unit() {
		while (next_bits(24) != 0x000001 &&
			next_bits(32) != 0x00000001)
			read_bits(8);//leading_zero_8bits /* equal to 0x00 */ f(8)
		if (next_bits(24) != 0x000001)
			read_bits(8);//zero_byte /* equal to 0x00 */ f(8)
		read_bits(24);//start_code_prefix_one_3bytes /* equal to 0x000001 */ f(24)
		nal_unit();

		while (more_data_in_byte_stream() &&
			next_bits(24) != 0x000001 &&
			next_bits(32) != 0x00000001)
			read_bits(8); //trailing_zero_8bits /* equal to 0x00 */ f(8)
	}
	//7.3.1 NAL unit syntax
	uint32_t nal_ref_idc = 0;
	// Table 7-1 – NAL unit type codes, syntax element categories, and NAL unit type classes
	uint32_t nal_unit_type = 0;
	uint32_t nalUnitHeaderBytes = 0;
	bool svc_extension_flag = false;
	bool avc_3d_extension_flag = false;
	void nal_unit() {
		read_bits(1);//	forbidden_zero_bit All f(1)
		nal_ref_idc = read_bits(2);//	nal_ref_idc All u(2)
		nal_unit_type = read_bits(5);//	nal_unit_type All u(5)
		if(nal_unit_type != 1)
		cout << "NLU " << cc++ << " TYPE " << nal_unit_type << endl;

		nalUnitHeaderBytes = 1;
		if (nal_unit_type == 14 || nal_unit_type == 20 || nal_unit_type == 21) {
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
		RBSP = vector<uint8_t>();
		while (more_data_in_byte_stream() &&
			next_bits(24) != 0x000001 &&
			next_bits(32) != 0x00000001) {
			if (next_bits(24) == 0x000003) {
				RBSP.push_back(read_bits(8));//rbsp_byte[NumBytesInRBSP++]  All b(8)
				RBSP.push_back(read_bits(8));//rbsp_byte[NumBytesInRBSP++]  All b(8)
				read_bits(8);// emulation_prevention_three_byte /* equal to 0x03 */ All f(8)

			}
			else
				RBSP.push_back(read_bits(8)); //rbsp_byte[NumBytesInRBSP++] All b(8)
		}
		uint32_t fileIndex = index;
		uint32_t fileBits = bits;
		uint32_t fileBitsIndex = bitsIndex;

		byteStream = &RBSP;
		bitsInit();
		typeHandler();
		byteStream = &fileByteStream;
		bitsInit(fileIndex, fileBits, fileBitsIndex);
	}

	void typeHandler() {
		switch (nal_unit_type)
		{
		case 1:
			//slice_layer_without_partitioning_rbsp();
			break;
		case 5:
			//slice_layer_without_partitioning_rbsp();
			break;
		case 6:
			sei_rbsp();
			break;
		case 7:
			seq_parameter_set_rbsp();
			break;
		case 8:
			pic_parameter_set_rbsp();
			break;
		default:
			break;
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
	bool non_idr_flag = false;
	uint32_t priority_id = 0;
	uint32_t view_id = 0;
	uint32_t temporal_id = 0;
	bool anchor_pic_flag = false;
	bool inter_view_flag = false;
	bool reserved_one_bit = false;
	void nal_unit_header_mvc_extension() {
		non_idr_flag = read_bits(1);
		priority_id = read_bits(6);
		view_id = read_bits(10);
		temporal_id = read_bits(3);
		anchor_pic_flag = read_bits(1);
		inter_view_flag = read_bits(1);
		reserved_one_bit = read_bits(1);
	}
#pragma endregion

#pragma region bits
	vector<uint8_t> fileByteStream;
	vector<uint8_t>* byteStream;
	vector<uint8_t> RBSP;
	size_t index;
	uint32_t  bits = 0;
	size_t bitsIndex = 0;
	bool RBSPMode = false;
	void bitsInit(size_t i=0,uint32_t b=0,size_t bi=32) {
		index = i;
		bitsIndex = bi;
		bits = b;
		while (bitsIndex >= 8 && more_data_in_byte_stream()) {
			bits = (bits << 8) | (*byteStream)[index++];
			bitsIndex -= 8;
		}
	}
	// 7.2 Specification of syntax functions, categories, and descriptors
	bool byte_aligned() {
		return bitsIndex % 8 == 0;
	}
	bool more_data_in_byte_stream() {
		return (index < (*byteStream).size());
	}
	bool more_rbsp_data() {
		return more_data_in_byte_stream();
	}
	bool more_rbsp_trailing_data() {
		return RBSP.size() > 0;
	}
	const uint32_t next_bits(size_t n) {
		if (n == 0) return 0;
		return (bits << bitsIndex) >> (32 - n);
	}
	// f(n)  u(n) b(8) p.68
	const uint32_t read_bits(size_t n) {
		if (n == 0) return 0;
		const uint32_t num = next_bits(n);
		bitsIndex += n;
		while (bitsIndex >= 8 && more_data_in_byte_stream()) {
			bits = (bits << 8) | (*byteStream)[index++];
			bitsIndex -= 8;
		}
		return num;
	}
	//9.1 Parsing process for Exp-Golomb codes
	uint32_t ue() {
		int leadingZeroBits = -1;
		for (uint8_t b = 0; !b; leadingZeroBits++)
			b = read_bits(1);
		uint32_t codeNum = (1 << leadingZeroBits) - 1 + read_bits(leadingZeroBits);
		return codeNum;
	}
	//9.1.1 Mapping process for signed Exp - Golomb codes
	int32_t se() {
		uint32_t  k = ue();
		return (k % 2 == 1 ? 1 : -1) * ceil(k / 2);
	}
	//Table 9-4 – Assignment of codeNum to values of coded_block_pattern for macroblock prediction modes
	// (a) ChromaArrayType is equal to 1 or 2
	const uint32_t table94a[48][2] = {
	{47, 0},
	{31,16 },
	{15,1 },
	{0,2 },
	{23,4 },
	{27,8 },
	{29,32 },
	{30,3 },
	{7,5 },
	{11,10 },
	{13,12 },
	{14,15 },
	{39,47 },
	{43,7 },
	{45,11 },
	{46,13 },
	{16,14 },
	{3,6 },
	{5,9 },
	{10,31 },
	{12,35 },
	{19,37 },
	{21,42 },
	{26,44 },
	{28,33 },
	{35,34 },
	{37,36 },
	{42,40 },
	{44,39 },
	{1,43 },
	{2,45 },
	{4,46 },
	{8,17 },
	{17,18 },
	{18,20 },
	{20,24 },
	{24,19 },
	{6,21 },
	{9,26 },
	{22,28 },
	{25,23 },
	{32,27 },
	{33,29 },
	{34,30 },
	{36,22 },
	{40,25 },
	{38,38 },
	{41,41 }, };
	// (b) ChromaArrayType is equal to 0 or 3
	const uint32_t table94b[16][2] = {
	{15,0},
	{0,1},
	{7,2},
	{11,4},
	{13,8},
	{14,3},
	{3,5},
	{5,10},
	{10,12},
	{12,15},
	{1,7},
	{2,11},
	{4,13},
	{8,14},
	{6,6},
	{9,9},
	};

	// 9.1.2 Mapping process for coded block pattern
	uint32_t me() {
		//uint32_t  codeNum = ue();
		//uint32_t predMode =  MbPartPredMode(mb_type, 0); 
		//bool isInter = predMode == Inter;
		//if (ChromaArrayType ==  1 || ChromaArrayType == 2) {
		//	return table94a[][]
		//}
		return 0;
	}
	// 9.3 CABAC parsing process for slice data
	int32_t ae() {
		//TODO
		return 0;
	}
	// 7.3.2 Raw byte sequence payloads and RBSP trailing bits syntax
	// 7.3.2.1 Sequence parameter set RBSP syntax
	void seq_parameter_set_rbsp() {
		seq_parameter_set_data();// 0
		rbsp_trailing_bits();// 0
	}
	// 7.3.2.1.1 Sequence parameter set data syntax
	uint8_t profile_idc = 0;
	bool constraint_set0_flag = false;
	bool constraint_set1_flag = false;
	bool constraint_set2_flag = false;
	bool constraint_set3_flag = false;
	bool constraint_set4_flag = false;
	bool constraint_set5_flag = false;
	bool separate_colour_plane_flag = false;
	uint32_t reserved_zero_2bits = 0;
	uint32_t chroma_format_idc = 0;
	uint8_t level_idc = 0;
	uint32_t seq_parameter_set_id = 0;
	uint32_t bit_depth_luma_minus8 = 0;
	uint32_t bit_depth_chroma_minus8 = 0;
	bool qpprime_y_zero_transform_bypass_flag = 0;
	bool seq_scaling_matrix_present_flag = 0;
	vector<bool> seq_scaling_list_present_flag;
	uint32_t log2_max_frame_num_minus4 = 0;
	uint32_t pic_order_cnt_type = 0;
	uint32_t log2_max_pic_order_cnt_lsb_minus4 = 0;
	bool delta_pic_order_always_zero_flag = false;
	int32_t offset_for_non_ref_pic = 0;
	int32_t offset_for_top_to_bottom_field = 0;
	uint32_t num_ref_frames_in_pic_order_cnt_cycle = 0;
	vector<int32_t> offset_for_ref_frame;
	uint32_t max_num_ref_frames = 0;
	bool gaps_in_frame_num_value_allowed_flag = false;
	uint32_t pic_width_in_mbs_minus1 = 0;
	uint32_t pic_height_in_map_units_minus1 = 0;
	bool frame_mbs_only_flag = false;
	bool mb_adaptive_frame_field_flag = false;
	bool direct_8x8_inference_flag = false;
	bool frame_cropping_flag = false;
	uint32_t frame_crop_left_offset = 0;
	uint32_t frame_crop_right_offset = 0;
	uint32_t frame_crop_top_offset = 0;
	uint32_t frame_crop_bottom_offset = 0;
	uint32_t ScalingList4x4[12][16] = { 0 };
	uint32_t ScalingList8x8[12][64] = { 0 };
	bool UseDefaultScalingMatrix4x4Flag[12] = { false };
	bool UseDefaultScalingMatrix8x8Flag[12] = { false };
	void seq_parameter_set_data() {
		profile_idc = read_bits(8);//profile_idc 0 u(8)
		constraint_set0_flag = read_bits(1);//constraint_set0_flag 0 u(1)
		constraint_set1_flag = read_bits(1);//constraint_set1_flag 0 u(1)
		constraint_set2_flag = read_bits(1);//constraint_set2_flag 0 u(1)
		constraint_set3_flag = read_bits(1);//constraint_set3_flag 0 u(1)
		constraint_set4_flag = read_bits(1);//constraint_set4_flag 0 u(1)
		constraint_set5_flag = read_bits(1);//constraint_set5_flag 0 u(1)
		reserved_zero_2bits = read_bits(2);//reserved_zero_2bits /* equal to 0 */ 0 u(2)
		assert(reserved_zero_2bits == 0);
		level_idc = read_bits(8);//level_idc 0 u(8)

		seq_parameter_set_id = ue();//seq_parameter_set_id 0 ue(v)
		if (profile_idc == 100 || profile_idc == 110 ||
			profile_idc == 122 || profile_idc == 244 || profile_idc == 44 ||
			profile_idc == 83 || profile_idc == 86 || profile_idc == 118 ||
			profile_idc == 128 || profile_idc == 138 || profile_idc == 139 ||
			profile_idc == 134 || profile_idc == 135) {
			chroma_format_idc = ue();// chroma_format_idc 0 ue(v)
			if (chroma_format_idc == 3)
				separate_colour_plane_flag = read_bits(1);// separate_colour_plane_flag 0 u(1)
			bit_depth_luma_minus8 = ue();//bit_depth_luma_minus8 0 ue(v)
			bit_depth_chroma_minus8 = ue();//bit_depth_chroma_minus8 0 ue(v)
			qpprime_y_zero_transform_bypass_flag = read_bits(1);//qpprime_y_zero_transform_bypass_flag 0 u(1)
			seq_scaling_matrix_present_flag = read_bits(1);//seq_scaling_matrix_present_flag 0 u(1)
			if (seq_scaling_matrix_present_flag) {
				seq_scaling_list_present_flag = vector<bool>();
				for (size_t i = 0; i < ((chroma_format_idc != 3) ? 8 : 12); i++) {
					seq_scaling_list_present_flag.push_back(read_bits(1));//seq_scaling_list_present_flag[i] 0 u(1)
					if (seq_scaling_list_present_flag[i])
						if (i < 6)
							scaling_list(ScalingList4x4[i], 16, UseDefaultScalingMatrix4x4Flag[i]);//0
						else
							scaling_list(ScalingList8x8[i - 6], 64, UseDefaultScalingMatrix8x8Flag[i - 6]);//0
				}
			}
		}
		log2_max_frame_num_minus4 = ue();//log2_max_frame_num_minus4 0 ue(v)
		pic_order_cnt_type = ue();//	pic_order_cnt_type 0 ue(v)
		if (pic_order_cnt_type == 0)
			log2_max_pic_order_cnt_lsb_minus4 = ue();// log2_max_pic_order_cnt_lsb_minus4 0 ue(v)
		else if (pic_order_cnt_type == 1) {
			delta_pic_order_always_zero_flag = read_bits(1);//delta_pic_order_always_zero_flag 0 u(1)
			offset_for_non_ref_pic = se();//offset_for_non_ref_pic 0 se(v)
			offset_for_top_to_bottom_field = se();//offset_for_top_to_bottom_field 0 se(v)
			num_ref_frames_in_pic_order_cnt_cycle = ue(); //num_ref_frames_in_pic_order_cnt_cycle 0 ue(v)

			offset_for_ref_frame = vector<int32_t>();
			for (size_t i = 0; i < num_ref_frames_in_pic_order_cnt_cycle; i++)
				offset_for_ref_frame.push_back(se());//offset_for_ref_frame[i] 0 se(v)
		}
		max_num_ref_frames = ue();//max_num_ref_frames 0 ue(v)
		gaps_in_frame_num_value_allowed_flag = read_bits(1);//	gaps_in_frame_num_value_allowed_flag 0 u(1)
		pic_width_in_mbs_minus1 = ue();//	pic_width_in_mbs_minus1 0 ue(v)
		pic_height_in_map_units_minus1 = ue();//	pic_height_in_map_units_minus1 0 ue(v)
		frame_mbs_only_flag = read_bits(1);//	frame_mbs_only_flag 0 u(1)
		if (!frame_mbs_only_flag)
			mb_adaptive_frame_field_flag = read_bits(1);//		mb_adaptive_frame_field_flag 0 u(1)
		direct_8x8_inference_flag = read_bits(1);//		direct_8x8_inference_flag 0 u(1)
		frame_cropping_flag = read_bits(1);//		frame_cropping_flag 0 u(1)
		if (frame_cropping_flag) {
			frame_crop_left_offset = ue();//	frame_crop_left_offset 0 ue(v)
			frame_crop_right_offset = ue();//	frame_crop_right_offset 0 ue(v)
			frame_crop_top_offset = ue();//	frame_crop_top_offset 0 ue(v)
			frame_crop_bottom_offset = ue();//	frame_crop_bottom_offset 0 ue(v)
		}
		bool vui_parameters_present_flag = read_bits(1);//vui_parameters_present_flag 0 u(1)
		if (vui_parameters_present_flag)
			vui_parameters();//		vui_parameters() 0
	}
	// 7.3.2.1.1.1 Scaling list syntax
	void scaling_list(uint32_t* scalingList, size_t sizeOfScalingList, bool& useDefaultScalingMatrixFlag) {
		size_t lastScale = 8;
		size_t	nextScale = 8;
		for (size_t j = 0; j < sizeOfScalingList; j++) {
			if (nextScale != 0) {
				int32_t delta_scale = se();//delta_scale 0 | 1 se(v)
				nextScale = (lastScale + delta_scale + 256) % 256;
				useDefaultScalingMatrixFlag = (j == 0 && nextScale == 0);
			}
			scalingList[j] = (nextScale == 0) ? lastScale : nextScale;
			lastScale = scalingList[j];
		}
	}
	// 7.3.2.1.2 Sequence parameter set extension RBSP syntax
	//uint32_t seq_parameter_set_id = 0;
	uint32_t aux_format_idc = 0;
	uint32_t bit_depth_aux_minus8 = 0;
	bool alpha_incr_flag = false;
	uint32_t alpha_opaque_value = 0;
	uint32_t alpha_transparent_value = 0;
	bool additional_extension_flag = false;
	void 	seq_parameter_set_extension_rbsp() {
		seq_parameter_set_id = ue();
		aux_format_idc = ue();
		if (aux_format_idc != 0) {
			bit_depth_aux_minus8 = ue();
			alpha_incr_flag = read_bits(1);
			alpha_opaque_value = read_bits(bit_depth_aux_minus8 + 9);
			alpha_transparent_value = read_bits(bit_depth_aux_minus8 + 9);
		}
		additional_extension_flag = read_bits(1);
		rbsp_trailing_bits();// rbsp_trailing_bits() 10
	}
	// 7.3.2.1.3 Subset sequence parameter set RBSP syntax
	bool svc_vui_parameters_present_flag = false;
	bool mvc_vui_parameters_present_flag = false;
	bool additional_extension2_flag = false;
	bool additional_extension2_data_flag = false;
	void subset_seq_parameter_set_rbsp() {
		seq_parameter_set_data();// seq_parameter_set_data() 0
		if (profile_idc == 83 || profile_idc == 86) {
			seq_parameter_set_svc_extension();// seq_parameter_set_svc_extension() /* specified in Annex F */ 0
			svc_vui_parameters_present_flag = read_bits(1); //	svc_vui_parameters_present_flag 0 u(1)
			if (svc_vui_parameters_present_flag == 1)
				svc_vui_parameters_extension();// svc_vui_parameters_extension() /* specified in Annex F */ 0
		}
		else if (profile_idc == 118 || profile_idc == 128 ||
			profile_idc == 134) {
			read_bits(1);// bit_equal_to_one /* equal to 1 */ 0 f(1)
			seq_parameter_set_mvc_extension();//	seq_parameter_set_mvc_extension() /* specified in Annex G */ 0
			mvc_vui_parameters_present_flag = read_bits(1);//mvc_vui_parameters_present_flag 0 u(1)
			if (mvc_vui_parameters_present_flag == 1)
				mvc_vui_parameters_extension();//mvc_vui_parameters_extension() /* specified in Annex G */ 0
		}
		else if (profile_idc == 138 || profile_idc == 135) {
			read_bits(1); //bit_equal_to_one /* equal to 1 */ 0 f(1)
			seq_parameter_set_mvcd_extension();//	seq_parameter_set_mvcd_extension() /* specified in Annex H */
		}
		else if (profile_idc == 139) {
			read_bits(1); //bit_equal_to_one /* equal to 1 */ 0 f(1)
			seq_parameter_set_mvcd_extension();//	seq_parameter_set_mvcd_extension() /* specified in Annex H */ 0
			seq_parameter_set_3davc_extension();//	seq_parameter_set_3davc_extension() /* specified in Annex I */ 0
		}
		additional_extension2_flag = read_bits(1);// additional_extension2_flag 0 u(1)
		if (additional_extension2_flag == 1)
			while (more_rbsp_data())
				additional_extension2_data_flag = read_bits(1);// additional_extension2_data_flag 0 u(1)
		rbsp_trailing_bits();//	rbsp_trailing_bits() 0
	}
	// 7.3.2.2 Picture parameter set RBSP syntax
	uint32_t pic_parameter_set_id = 0;
	//uint32_t seq_parameter_set_id = 0;
	bool entropy_coding_mode_flag = false;
	bool bottom_field_pic_order_in_frame_present_flag = false;
	uint32_t num_slice_groups_minus1 = 0;
	uint32_t slice_group_map_type = 0;
	vector<uint32_t> run_length_minus1 = vector<uint32_t>();
	vector<uint32_t> top_left = vector<uint32_t>();
	vector<uint32_t> bottom_right = vector<uint32_t>();
	bool slice_group_change_direction_flag = false;
	uint32_t slice_group_change_rate_minus1 = 0;
	uint32_t pic_size_in_map_units_minus1 = 0;
	vector<uint32_t> slice_group_id = vector<uint32_t>();
	uint32_t num_ref_idx_l0_default_active_minus1 = 0;
	uint32_t num_ref_idx_l1_default_active_minus1 = 0;
	bool weighted_pred_flag = false;
	uint32_t weighted_bipred_idc = 0;
	int32_t pic_init_qp_minus26 = 0;
	int32_t pic_init_qs_minus26 = 0;
	int32_t chroma_qp_index_offset = 0;
	bool deblocking_filter_control_present_flag = false;
	bool constrained_intra_pred_flag = false;
	bool redundant_pic_cnt_present_flag = false;
	bool transform_8x8_mode_flag = false;
	bool pic_scaling_matrix_present_flag = false;
	vector<bool> pic_scaling_list_present_flag = vector<bool>();
	int32_t second_chroma_qp_index_offset = 0;
	void pic_parameter_set_rbsp() {
		pic_parameter_set_id = ue();
		seq_parameter_set_id = ue();
		entropy_coding_mode_flag = read_bits(1);
		bottom_field_pic_order_in_frame_present_flag = read_bits(1);
		num_slice_groups_minus1 = ue();
		assert(num_slice_groups_minus1 == 0);
		if (num_slice_groups_minus1 > 0) {
			slice_group_map_type = ue();
			if (slice_group_map_type == 0)
				for (size_t iGroup = 0; iGroup <= num_slice_groups_minus1; iGroup++)
					run_length_minus1.push_back(ue());
			else if (slice_group_map_type == 2)
				for (size_t iGroup = 0; iGroup < num_slice_groups_minus1; iGroup++) {
					top_left.push_back(ue());
					bottom_right.push_back(ue());
				}
			else if (slice_group_map_type == 3 ||
				slice_group_map_type == 4 ||
				slice_group_map_type == 5) {
				slice_group_change_direction_flag = read_bits(1);
				slice_group_change_rate_minus1 = ue();
			}
			else if (slice_group_map_type == 6) {
				pic_size_in_map_units_minus1 = ue();
				for (size_t i = 0; i <= pic_size_in_map_units_minus1; i++)
					slice_group_id.push_back(read_bits(ceil(log2(num_slice_groups_minus1 + 1)))); //Ceil( Log2( num_slice_groups_minus1 + 1 ) ) bits.
			}
		}
		num_ref_idx_l0_default_active_minus1 = ue();
		num_ref_idx_l1_default_active_minus1 = ue();
		weighted_pred_flag = read_bits(1);
		weighted_bipred_idc = read_bits(2);
		pic_init_qp_minus26 = se();
		pic_init_qs_minus26 = se();
		chroma_qp_index_offset = se();
		deblocking_filter_control_present_flag = read_bits(1);
		constrained_intra_pred_flag = read_bits(1);
		redundant_pic_cnt_present_flag = read_bits(1);
		assert(redundant_pic_cnt_present_flag == 0);
		if (more_rbsp_data()) {  // shall not
			transform_8x8_mode_flag = read_bits(1);
			pic_scaling_matrix_present_flag = read_bits(1);
			if (pic_scaling_matrix_present_flag)
				for (size_t i = 0; i < 6 + ((chroma_format_idc != 3) ? 2 : 6) * transform_8x8_mode_flag; i++) {
					pic_scaling_list_present_flag.push_back(read_bits(1));
					if (pic_scaling_list_present_flag[i])
						if (i < 6)
							scaling_list(ScalingList4x4[i], 16, UseDefaultScalingMatrix4x4Flag[i]);

						else
							scaling_list(ScalingList8x8[i - 6], 64, UseDefaultScalingMatrix8x8Flag[i - 6]);

				}
			second_chroma_qp_index_offset = se();
		}
		rbsp_trailing_bits();
	}
	// 7.3.2.3 Supplemental enhancement information RBSP syntax
	void sei_rbsp() {
		cout << "[SKIP SEI]" << endl;
		// SKIP SEI
	}
	// 7.3.2.3.1 Supplemental enhancement information message syntax
	// F.7.3.2.1.4 Sequence parameter set SVC extension syntax
	bool inter_layer_deblocking_filter_control_present_flag = false;
	uint8_t extended_spatial_scalability_idc = 0;
	bool chroma_phase_x_plus1_flag = false;
	uint32_t chroma_phase_y_plus1 = 0;
	bool seq_ref_layer_chroma_phase_x_plus1_flag = false;
	uint32_t seq_ref_layer_chroma_phase_y_plus1 = 0;
	int32_t seq_scaled_ref_layer_left_offset = 0;
	int32_t seq_scaled_ref_layer_top_offset = 0;
	int32_t seq_scaled_ref_layer_right_offset = 0;
	int32_t seq_scaled_ref_layer_bottom_offset = 0;
	bool seq_tcoeff_level_prediction_flag = false;
	bool adaptive_tcoeff_level_prediction_flag = false;
	bool slice_header_restriction_flag = false;
	void seq_parameter_set_svc_extension() {
		inter_layer_deblocking_filter_control_present_flag = read_bits(1);// 0 u(1)
		extended_spatial_scalability_idc = read_bits(2);// extended_spatial_scalability_idc 0 u(2)
		if (ChromaArrayType == 1 || ChromaArrayType == 2)
			chroma_phase_x_plus1_flag = read_bits(1);// chroma_phase_x_plus1_flag 0 u(1)
		if (ChromaArrayType == 1)
			chroma_phase_y_plus1 = read_bits(2);//chroma_phase_y_plus1 0 u(2)
		if (extended_spatial_scalability_idc == 1) {
			if (ChromaArrayType > 0) {
				seq_ref_layer_chroma_phase_x_plus1_flag = false;// seq_ref_layer_chroma_phase_x_plus1_flag 0 u(1)
				seq_ref_layer_chroma_phase_y_plus1 = read_bits(2);//	seq_ref_layer_chroma_phase_y_plus1 0 u(2)
			}
			seq_scaled_ref_layer_bottom_offset = se();// seq_scaled_ref_layer_left_offset 0 se(v)
			seq_scaled_ref_layer_top_offset = se();// 	seq_scaled_ref_layer_top_offset 0 se(v)
			seq_scaled_ref_layer_right_offset = se();// 	seq_scaled_ref_layer_right_offset 0 se(v)
			seq_scaled_ref_layer_bottom_offset = se();// 	seq_scaled_ref_layer_bottom_offset 0 se(v)
		}
		seq_tcoeff_level_prediction_flag = read_bits(1);// seq_tcoeff_level_prediction_flag 0 u(1)
		if (seq_tcoeff_level_prediction_flag) {
			adaptive_tcoeff_level_prediction_flag = read_bits(1);//adaptive_tcoeff_level_prediction_flag 0 u(1)
		}
		slice_header_restriction_flag = read_bits(1);//slice_header_restriction_flag 0 u(1)
	}
	// F.14.1 SVC VUI parameters extension syntax
	uint32_t vui_ext_num_entries_minus1 = 0;
	vector<uint32_t> vui_ext_dependency_id;
	vector<uint32_t>	vui_ext_quality_id;
	vector<uint32_t>	vui_ext_temporal_id;
	vector<bool>	vui_ext_timing_info_present_flag;
	vector<uint32_t>	vui_ext_num_units_in_tick;
	vector<uint32_t>	vui_ext_time_scale;
	vector<bool>	vui_ext_fixed_frame_rate_flag;
	vector<bool> vui_ext_nal_hrd_parameters_present_flag;
	vector<bool> vui_ext_vcl_hrd_parameters_present_flag;
	vector<bool> vui_ext_low_delay_hrd_flag;
	vector<bool> vui_ext_pic_struct_present_flag;
	void svc_vui_parameters_extension() {
		vui_ext_num_entries_minus1 = ue();// vui_ext_num_entries_minus1 0 ue(v)
		vui_ext_dependency_id = vector<uint32_t>();
		vui_ext_quality_id = vector<uint32_t>();
		vui_ext_temporal_id = vector<uint32_t>();
		vui_ext_timing_info_present_flag = vector<bool>();
		vui_ext_num_units_in_tick = vector<uint32_t>();
		vui_ext_time_scale = vector<uint32_t>();
		vui_ext_fixed_frame_rate_flag = vector<bool>();
		vui_ext_nal_hrd_parameters_present_flag = vector<bool>();
		vui_ext_vcl_hrd_parameters_present_flag = vector<bool>();
		vui_ext_low_delay_hrd_flag = vector<bool>();
		vui_ext_pic_struct_present_flag = vector<bool>();
		for (size_t i = 0; i <= vui_ext_num_entries_minus1; i++) {
			vui_ext_dependency_id.push_back(read_bits(3));// vui_ext_dependency_id[i] 0 u(3)
			vui_ext_quality_id.push_back(read_bits(4));//	vui_ext_quality_id[i] 0 u(4)
			vui_ext_temporal_id.push_back(read_bits(3));//vui_ext_temporal_id[i] 0 u(3)
			vui_ext_timing_info_present_flag.push_back(read_bits(1)); //vui_ext_timing_info_present_flag[i] 0 u(1)
			if (vui_ext_timing_info_present_flag[i]) {
				vui_ext_num_units_in_tick.push_back(read_bits(32));// vui_ext_num_units_in_tick[i] 0 u(32)
				vui_ext_time_scale.push_back(read_bits(32));//	vui_ext_time_scale[i] 0 u(32)
				vui_ext_fixed_frame_rate_flag.push_back(read_bits(1));//	vui_ext_fixed_frame_rate_flag[i] 0 u(1)
			}
			vui_ext_nal_hrd_parameters_present_flag.push_back(read_bits(1));//	vui_ext_nal_hrd_parameters_present_flag[i] 0 u(1)
			if (vui_ext_nal_hrd_parameters_present_flag[i])
				hrd_parameters();//hrd_parameters() 0
			vui_ext_vcl_hrd_parameters_present_flag.push_back(read_bits(1));//	vui_ext_nal_hrd_parameters_present_flag[i] 0 u(1)
			if (vui_ext_vcl_hrd_parameters_present_flag[i])
				hrd_parameters();//hrd_parameters() 0
			if (vui_ext_nal_hrd_parameters_present_flag[i] ||
				vui_ext_vcl_hrd_parameters_present_flag[i])
				vui_ext_low_delay_hrd_flag.push_back(read_bits(1));//	vui_ext_low_delay_hrd_flag[i] 0 u(1)
			vui_ext_pic_struct_present_flag.push_back(read_bits(1));//	vui_ext_pic_struct_present_flag[i] 0 u(1)
		}
	}
	// 7.3.2.8 Slice layer without partitioning RBSP syntax
	void slice_layer_without_partitioning_rbsp() {
		slice_header();
		slice_data();// /* all categories of slice_data( ) syntax */ 2 | 3 | 4
		//rbsp_slice_trailing_bits();// 2
	}
	// 7.3.2.11 RBSP trailing bits syntax
	void rbsp_trailing_bits() {
		read_bits(1);//rbsp_stop_one_bit /* equal to 1 */ All f(1)
		while (!byte_aligned())
			read_bits(1);//rbsp_alignment_zero_bit /* equal to 0 */ All f(1)
	}
	// 7.3.3 Slice header syntax
	uint32_t first_mb_in_slice = 0;
	// Table 7-6 – Name association to slice_type
	uint32_t slice_type = 0;
	//uint32_t pic_parameter_set_id = 0;
	uint32_t colour_plane_id = 0;
	uint32_t frame_num = 0;
	bool field_pic_flag = false;
	bool bottom_field_flag = false;
	uint32_t idr_pic_id = 0;
	uint32_t pic_order_cnt_lsb = 0;
	int32_t delta_pic_order_cnt_bottom = 0;
	vector<int32_t> delta_pic_order_cnt = vector<int32_t>();
	uint32_t redundant_pic_cnt = 0;
	bool direct_spatial_mv_pred_flag = false;
	bool num_ref_idx_active_override_flag = false;
	uint32_t num_ref_idx_l0_active_minus1 = 0;
	uint32_t num_ref_idx_l1_active_minus1 = 0;
	uint32_t cabac_init_idc = 0;
	int32_t slice_qp_delta = 0;
	bool sp_for_switch_flag = false;
	int32_t slice_qs_delta = 0;
	uint32_t disable_deblocking_filter_idc = 0;
	int32_t slice_alpha_c0_offset_div2 = 0;
	int32_t slice_beta_offset_div2 = 0;
	uint32_t slice_group_change_cycle = 0;
	void slice_header() {
		first_mb_in_slice = ue();
		slice_type = ue();
		pic_parameter_set_id = ue();
		if (separate_colour_plane_flag == 1)
			colour_plane_id = read_bits(2);
		frame_num = read_bits(log2_max_frame_num_minus4 + 4);
		if (!frame_mbs_only_flag) {
			field_pic_flag = read_bits(1);
			if (field_pic_flag)
				bottom_field_flag = read_bits(1);
		}
		if (IdrPicFlag)
			idr_pic_id = ue();
		if (pic_order_cnt_type == 0) {
			pic_order_cnt_lsb = read_bits(log2_max_pic_order_cnt_lsb_minus4 + 4);
			if (bottom_field_pic_order_in_frame_present_flag && !field_pic_flag)
				delta_pic_order_cnt_bottom = se();

		}
		if (pic_order_cnt_type == 1 && !delta_pic_order_always_zero_flag) {
			delta_pic_order_cnt.push_back(se());
			if (bottom_field_pic_order_in_frame_present_flag && !field_pic_flag)
				delta_pic_order_cnt.push_back(se());
		}
		if (redundant_pic_cnt_present_flag)
			redundant_pic_cnt = ue();
		if (sliceTypeCheck(B))
			direct_spatial_mv_pred_flag = read_bits(1);
		if (sliceTypeCheck(P) || sliceTypeCheck(SP) || sliceTypeCheck(B)) {
			num_ref_idx_active_override_flag = read_bits(1);
			if (num_ref_idx_active_override_flag) {
				num_ref_idx_l0_active_minus1 = ue();
				if (sliceTypeCheck(B))
					num_ref_idx_l1_active_minus1 = ue();
			}
		}
		if (nal_unit_type == 20 || nal_unit_type == 21)
			ref_pic_list_mvc_modification();// /* specified in Annex G */ 2
		else
			ref_pic_list_modification();
		if ((weighted_pred_flag && (sliceTypeCheck(P) || sliceTypeCheck(SP))) ||
			(weighted_bipred_idc == 1 && sliceTypeCheck(B)))
			pred_weight_table();
		if (nal_ref_idc != 0)
			dec_ref_pic_marking();
		if (entropy_coding_mode_flag && sliceTypeCheck(I) && sliceTypeCheck(SI))
			cabac_init_idc = ue();
		slice_qp_delta = se();
		if (sliceTypeCheck(SP) || sliceTypeCheck(SI)) {
			if (sliceTypeCheck(SP))
				sp_for_switch_flag = read_bits(1);
			slice_qs_delta = se();
		}
		if (deblocking_filter_control_present_flag) {
			disable_deblocking_filter_idc = ue();
			if (disable_deblocking_filter_idc != 1) {
				slice_alpha_c0_offset_div2 = se();
				slice_beta_offset_div2 = se();
			}
		}
		if (num_slice_groups_minus1 > 0 &&
			slice_group_map_type >= 3 && slice_group_map_type <= 5)
			slice_group_change_cycle = read_bits(ceil(log2(PicSizeInMapUnits / SliceGroupChangeRate + 1))); //Ceil( Log2( PicSizeInMapUnits ÷ SliceGroupChangeRate + 1 ) ) 
	}

	// 7.3.3.1 Reference picture list modification syntax
	bool ref_pic_list_modification_flag_l0 = false;
	bool ref_pic_list_modification_flag_l1 = false;
	uint32_t modification_of_pic_nums_idc = 0;
	uint32_t abs_diff_pic_num_minus1 = 0;
	uint32_t long_term_pic_num = 0;
	void ref_pic_list_modification() {
		if (slice_type % 5 != 2 && slice_type % 5 != 4) {
			ref_pic_list_modification_flag_l0 = read_bits(1);
			if (ref_pic_list_modification_flag_l0)
				do {
					modification_of_pic_nums_idc = ue();
					if (modification_of_pic_nums_idc == 0 ||
						modification_of_pic_nums_idc == 1)
						abs_diff_pic_num_minus1 = ue();
					else if (modification_of_pic_nums_idc == 2)
						long_term_pic_num = ue();
				} while (modification_of_pic_nums_idc != 3);
		}
		if (slice_type % 5 == 1) {
			ref_pic_list_modification_flag_l1 = read_bits(1);
			if (ref_pic_list_modification_flag_l1)
				do {
					modification_of_pic_nums_idc = ue();
					if (modification_of_pic_nums_idc == 0 ||
						modification_of_pic_nums_idc == 1)
						abs_diff_pic_num_minus1 = ue();
					else if (modification_of_pic_nums_idc == 2)
						long_term_pic_num = ue();
				} while (modification_of_pic_nums_idc != 3);
		}
	}
	// 7.3.3.2 Prediction weight table syntax
	uint32_t luma_log2_weight_denom = 0;
	uint32_t chroma_log2_weight_denom = 0;
	bool luma_weight_l0_flag = false;
	vector<int32_t> luma_weight_l0 = vector<int32_t>();
	vector<int32_t> luma_offset_l0 = vector<int32_t>();
	bool chroma_weight_l0_flag = false;
	vector<vector<int32_t>> chroma_weight_l0 = vector<vector<int32_t>>();
	vector<vector<int32_t>> chroma_offset_l0 = vector<vector<int32_t>>();
	bool luma_weight_l1_flag = false;
	vector<int32_t> luma_weight_l1 = vector<int32_t>();
	vector<int32_t> luma_offset_l1 = vector<int32_t>();
	bool chroma_weight_l1_flag = false;
	vector<vector<int32_t>> chroma_weight_l1 = vector<vector<int32_t>>();
	vector<vector<int32_t>> chroma_offset_l1 = vector<vector<int32_t>>();
	void pred_weight_table() {
		luma_log2_weight_denom = ue();
		if (ChromaArrayType != 0)
			chroma_log2_weight_denom = ue();
		for (size_t i = 0; i <= num_ref_idx_l0_active_minus1; i++) {
			luma_weight_l0_flag = read_bits(1);
			if (luma_weight_l0_flag) {
				luma_weight_l0.push_back(se());
				luma_offset_l0.push_back(se());
			}
			chroma_weight_l0.push_back(vector<int32_t>());
			chroma_offset_l0.push_back(vector<int32_t>());
			if (ChromaArrayType != 0) {
				chroma_weight_l0_flag = read_bits(1);
				if (chroma_weight_l0_flag)
					for (size_t j = 0; j < 2; j++) {
						chroma_weight_l0[i].push_back(se());
						chroma_offset_l0[i].push_back(se());
					}
			}
		}
		if (slice_type % 5 == 1)
			for (size_t i = 0; i <= num_ref_idx_l1_active_minus1; i++) {
				luma_weight_l1_flag = read_bits(1);
				if (luma_weight_l1_flag) {
					luma_weight_l1.push_back(se());
					luma_offset_l1.push_back(se());
				}
				chroma_weight_l1.push_back(vector<int32_t>());
				chroma_offset_l1.push_back(vector<int32_t>());
				if (ChromaArrayType != 0) {
					chroma_weight_l1_flag = read_bits(1);
					if (chroma_weight_l1_flag)
						for (size_t j = 0; j < 2; j++) {
							chroma_weight_l1[i].push_back(se());
							chroma_offset_l1[i].push_back(se());

						}
				}
			}
	}
	//7.3.3.3 Decoded reference picture marking syntax

	bool no_output_of_prior_pics_flag = false;
	bool long_term_reference_flag = false;
	bool adaptive_ref_pic_marking_mode_flag = false;
	uint32_t memory_management_control_operation = 0;
	uint32_t difference_of_pic_nums_minus1 = 0;
	uint32_t long_term_frame_idx = 0;
	uint32_t max_long_term_frame_idx_plus1 = 0;
	void dec_ref_pic_marking() {
		if (IdrPicFlag) {
			no_output_of_prior_pics_flag = read_bits(1);
			long_term_reference_flag = read_bits(1);
		}
		else {
			adaptive_ref_pic_marking_mode_flag = read_bits(1);
			if (adaptive_ref_pic_marking_mode_flag)
				do {
					memory_management_control_operation = ue();
					if (memory_management_control_operation == 1 ||
						memory_management_control_operation == 3)
						difference_of_pic_nums_minus1 = ue();
					if (memory_management_control_operation == 2)
						long_term_pic_num = ue();
					if (memory_management_control_operation == 3 ||
						memory_management_control_operation == 6)
						long_term_frame_idx = ue();
					if (memory_management_control_operation == 4)
						max_long_term_frame_idx_plus1 = ue();
				} while (memory_management_control_operation != 0);
		}
	}
	// 7.3.4 Slice data syntax
	bool cabac_alignment_one_bit = false;
	uint32_t mb_skip_run = 0;
	bool mb_skip_flag = 0;
	bool mb_field_decoding_flag = false;
	bool end_of_slice_flag = 0;
	uint32_t CurrMbAddr = 0;
	void slice_data() {
		if (entropy_coding_mode_flag)
			while (!byte_aligned())
				cabac_alignment_one_bit = read_bits(1);
		CurrMbAddr = first_mb_in_slice * (1 + MbaffFrameFlag);
		bool moreDataFlag = 1;
		bool prevMbSkipped = 0;
		calcMapUnitToSliceGroupMap();
		do {
			if (slice_type != I && slice_type != SI)
				if (!entropy_coding_mode_flag) {
					mb_skip_run = ue();
					prevMbSkipped = (mb_skip_run > 0);
					for (size_t i = 0; i < mb_skip_run; i++)
						CurrMbAddr = NextMbAddress(CurrMbAddr);
					if (mb_skip_run > 0)
						moreDataFlag = more_rbsp_data();
				}
				else {
					mb_skip_flag = ae();
					moreDataFlag = !mb_skip_flag;
				}
			if (moreDataFlag) {
				if (MbaffFrameFlag && (CurrMbAddr % 2 == 0 ||
					(CurrMbAddr % 2 == 1 && prevMbSkipped)))
					mb_field_decoding_flag = read_bits(1); // mb_field_decoding_flag 2 u(1) | ae(v)
				macroblock_layer();
			}
			if (!entropy_coding_mode_flag)
				moreDataFlag = more_rbsp_data();
			else {
				if (slice_type != I && slice_type != SI)
					prevMbSkipped = mb_skip_flag;
				if (MbaffFrameFlag && CurrMbAddr % 2 == 0)
					moreDataFlag = 1;
				else {
					end_of_slice_flag = ae();
					moreDataFlag = !end_of_slice_flag;
				}
			}
			CurrMbAddr = NextMbAddress(CurrMbAddr);
		} while (moreDataFlag);
	}
	// 7.3.5 Macroblock layer syntax
	uint32_t mb_type = I_NxN;
	bool pcm_alignment_zero_bit = false;
	vector<uint32_t> pcm_sample_luma = vector<uint32_t>();
	vector<uint32_t> pcm_sample_chroma = vector<uint32_t>();
	bool transform_size_8x8_flag = false;
	int32_t coded_block_pattern = 0;
	int32_t mb_qp_delta = 0;

	void macroblock_layer() {
		mb_type = ue();// ue(v) | ae(v)
		if (mb_type == I_PCM) {
			while (!byte_aligned())
				pcm_alignment_zero_bit = read_bits(1);
			for (size_t i = 0; i < 256; i++)
				pcm_sample_luma.push_back(read_bits(BitDepthY));
			for (size_t i = 0; i < 2 * MbWidthC * MbHeightC; i++)
				pcm_sample_chroma.push_back(read_bits(BitDepthC));
		}
		else {
			//bool noSubMbPartSizeLessThan8x8Flag = 1;
			//if (mb_type != I_NxN &&
			//	MbPartPredMode(mb_type, 0) != Intra_16x16 &&
			//	NumMbPart(mb_type) == 4) {
			//	sub_mb_pred(mb_type) 2
			//		for (mbPartIdx = 0; mbPartIdx < 4; mbPartIdx++)
			//			if (sub_mb_type[mbPartIdx] != B_Direct_8x8) {
			//				if (NumSubMbPart(sub_mb_type[mbPartIdx]) > 1)
			//					noSubMbPartSizeLessThan8x8Flag = 0
			//			}
			//			else if (!direct_8x8_inference_flag)
			//				noSubMbPartSizeLessThan8x8Flag = 0
			//}
			//else {
			//	if (transform_8x8_mode_flag&& mb_type = = I_NxN)
			//		transform_size_8x8_flag 2 u(1) | ae(v)
			//		mb_pred(mb_type) 2
			//}
			//if (MbPartPredMode(mb_type, 0) != Intra_16x16) {
			//	coded_block_pattern 2 me(v) | ae(v)
			//		if (CodedBlockPatternLuma > 0 &&
			//			transform_8x8_mode_flag && mb_type != I_NxN &&
			//			noSubMbPartSizeLessThan8x8Flag &&
			//			(mb_type != B_Direct_16x16 | | direct_8x8_inference_flag))
			//			transform_size_8x8_flag 2 u(1) | ae(v)
			//}
			//if (CodedBlockPatternLuma > 0 | | CodedBlockPatternChroma > 0 | |
			//	MbPartPredMode(mb_type, 0) = = Intra_16x16) {
			//	mb_qp_delta 2 se(v) | ae(v)
			//		residual(0, 15) 3 | 4
			//}
		}
	}
	// 7.3.5.1 Macroblock prediction syntax
	// Table 7-11 – Macroblock types for I slice
	// NumMbPart(mb_type)|MbPartPredMode(mb_type, 0)|MbPartPredMode	(mb_type, 1)|MbPartWidth(mb_type)|MbPartHeight(mb_type)
// Table 7-13 – Macroblock type values 0 to 4 for P and SP slices
	uint32_t TableP[6][5] = {
	{1,Pred_L0,na,16,16}	,
	{2,Pred_L0,Pred_L0,16,8},
	{2,Pred_L0,Pred_L0,8,16},
	{4,na,na,8,8}			,
	{4,na,na,8,8}			,
	{1,Pred_L0,na,16,16}	,
	};
	// Table 7 - 14 – Macroblock type values 0 to 22 for B slices
	uint32_t TableB[24][5] =
	{
	{na,Direct,na,8,8},
	{1,Pred_L0,na,16,16}	,
	{1,Pred_L1,na,16,16}	,
	{1,BiPred,na,16,16}		,
	{2,Pred_L0,Pred_L0,16,8},
	{2,Pred_L0,Pred_L0,8,16},
	{2,Pred_L1,Pred_L1,16,8},
	{2,Pred_L1,Pred_L1,8,16},
	{2,Pred_L0,Pred_L1,16,8},
	{2,Pred_L0,Pred_L1,8,16},
	{2,Pred_L1,Pred_L0,16,8},
	{2,Pred_L1,Pred_L0,8,16},
	{2,Pred_L0,BiPred,16,8}	,
	{2,Pred_L0,BiPred,8,16}	,
	{2,Pred_L1,BiPred,16,8}	,
	{2,Pred_L1,BiPred,8,16}	,
	{2,BiPred,Pred_L0,16,8}	,
	{2,BiPred,Pred_L0,8,16}	,
	{2,BiPred,Pred_L1,16,8}	,
	{2,BiPred,Pred_L1,8,16}	,
	{2,BiPred,BiPred,16,8}	,
	{2,BiPred,BiPred,8,16}	,
	{4,na,na,8,8}			,
	{na,Direct,na,8,8}		,
	};
	uint32_t MbPartPredMode(uint32_t mb_type, int n) {
		if (n == 0) {
			if (sliceTypeCheck(I)) {
				if (mb_type == I_NxN) return transform_size_8x8_flag == 0 ? Intra_4x4 : Intra_8x8;
				else if (mb_type == I_PCM) return na;
				else return Intra_16x16;
			}
			else if (sliceTypeCheck(P) || sliceTypeCheck(SP)) {
				return TableP[mb_type][1];
			}
			else if (sliceTypeCheck(B)) {
				return TableB[mb_type][1];
			}
		}
		return na;
	}


	uint32_t NumMbPart(int mb_type) {
		if (sliceTypeCheck(P) || sliceTypeCheck(SP)) {
			return TableP[mb_type][0];
		}
		else if (sliceTypeCheck(B)) {
			return TableB[mb_type][0];
		}
		return na;
	}


#pragma endregion
#pragma region 7.4.2 Raw byte sequence payloads and RBSP trailing bits semantics
	//– If separate_colour_plane_flag is equal to 0, ChromaArrayType is set equal to chroma_format_idc.
	//– Otherwise(separate_colour_plane_flag is equal to 1), ChromaArrayType is set equal to 0.
	uint32_t getChromaArrayType() {
		if (separate_colour_plane_flag == 0) return chroma_format_idc;
		return 0;
	}
#pragma endregion

#pragma region 8.2
	size_t NextMbAddress(uint32_t n) {
		size_t i = n + 1;
		while (i < PicSizeInMbs && MbToSliceGroupMap(i) != MbToSliceGroupMap(n))
			i++;
		return  i;
	}
	// 8.2.2.1 Specification for interleaved slice group map type
	vector<uint32_t> mapUnitToSliceGroupMap;
	void calcMapUnitToSliceGroupMap() {
		mapUnitToSliceGroupMap = vector<uint32_t>(PicSizeInMapUnits, 0);
		size_t i, j, k, x, y, iGroup;
		size_t sizeOfUpperLeftGroup =
			(slice_group_change_direction_flag ? (PicSizeInMapUnits - MapUnitsInSliceGroup0) : MapUnitsInSliceGroup0); // 8-14
		int leftBound = 0, topBound = 0, rightBound = 0, bottomBound = 0, xDir = 0, yDir = 0, mapUnitVacant = 0;

		switch (slice_group_map_type)
		{
		case 0: // (8-17)
			i = 0;
			do
				for (iGroup = 0; iGroup <= num_slice_groups_minus1 && i < PicSizeInMapUnits;
					i += run_length_minus1[iGroup++] + 1)
					for (j = 0; j <= run_length_minus1[iGroup] && i + j < PicSizeInMapUnits; j++)
						mapUnitToSliceGroupMap[i + j] = iGroup;
			while (i < PicSizeInMapUnits);
			break;
		case 1: // 8-18
			for (i = 0; i < PicSizeInMapUnits; i++)
				mapUnitToSliceGroupMap[i] = ((i % PicWidthInMbs) +
					(((i / PicWidthInMbs) * (num_slice_groups_minus1 + 1)) / 2))
				% (num_slice_groups_minus1 + 1);
			break;

		case 2: // 8-19
			for (i = 0; i < PicSizeInMapUnits; i++)
				mapUnitToSliceGroupMap[i] = num_slice_groups_minus1;
			for (iGroup = num_slice_groups_minus1 - 1; iGroup >= 0; iGroup--) {
				size_t yTopLeft = top_left[iGroup] / PicWidthInMbs;
				size_t xTopLeft = top_left[iGroup] % PicWidthInMbs;
				size_t yBottomRight = bottom_right[iGroup] / PicWidthInMbs;
				size_t xBottomRight = bottom_right[iGroup] % PicWidthInMbs;
				for (y = yTopLeft; y <= yBottomRight; y++)
					for (x = xTopLeft; x <= xBottomRight; x++)
						mapUnitToSliceGroupMap[y * PicWidthInMbs + x] = iGroup;
			}
			break;

		case 3: // 8-20
			for (i = 0; i < PicSizeInMapUnits; i++)
				mapUnitToSliceGroupMap[i] = 1;
			x = (PicWidthInMbs - slice_group_change_direction_flag) / 2;
			y = (PicHeightInMapUnits - slice_group_change_direction_flag) / 2;
			(leftBound, topBound) = (x, y);
			(rightBound, bottomBound) = (x, y);
			(xDir, yDir) = (slice_group_change_direction_flag - 1, slice_group_change_direction_flag);
			for (k = 0; k < MapUnitsInSliceGroup0; k += mapUnitVacant) {
				mapUnitVacant = (mapUnitToSliceGroupMap[y * PicWidthInMbs + x] == 1);
				if (mapUnitVacant)
					mapUnitToSliceGroupMap[y * PicWidthInMbs + x] = 0;
				if (xDir == -1 && x == leftBound) {
					leftBound = max(leftBound - 1, 0);
					x = leftBound;
					(xDir, yDir) = (0, 2 * slice_group_change_direction_flag - 1);
				}
				else if (xDir == 1 && x == rightBound) {
					rightBound = min(rightBound + 1, (int)PicWidthInMbs - 1);
					x = rightBound;
					(xDir, yDir) = (0, 1 - 2 * slice_group_change_direction_flag);
				}
				else if (yDir == -1 && y == topBound) {
					topBound = max(topBound - 1, 0);
					y = topBound;
					(xDir, yDir) = (1 - 2 * slice_group_change_direction_flag, 0);
				}
				else if (yDir == 1 && y == bottomBound) {
					bottomBound = min(bottomBound + 1, (int)PicHeightInMapUnits - 1);
					y = bottomBound;
					(xDir, yDir) = (2 * slice_group_change_direction_flag - 1, 0);
				}
				else
					(x, y) = (x + xDir, y + yDir);
			}
			break;
		case 4:// 8-21
			for (i = 0; i < PicSizeInMapUnits; i++)
				if (i < sizeOfUpperLeftGroup)
					mapUnitToSliceGroupMap[i] = slice_group_change_direction_flag;
				else
					mapUnitToSliceGroupMap[i] = 1 - slice_group_change_direction_flag;
			break;
		case 5:// 8-22
			k = 0;
			for (j = 0; j < PicWidthInMbs; j++)
				for (i = 0; i < PicHeightInMapUnits; i++)
					if (k++ < sizeOfUpperLeftGroup)
						mapUnitToSliceGroupMap[i * PicWidthInMbs + j] = slice_group_change_direction_flag;
					else
						mapUnitToSliceGroupMap[i * PicWidthInMbs + j] = 1 - slice_group_change_direction_flag;
			break;
		case 6:// 8-23
			for (i = 0; i < PicSizeInMapUnits; i++)
				mapUnitToSliceGroupMap[i] = slice_group_id[i];
			break;
		default:
			break;
		}
	}
#pragma endregion

#pragma region Annex E
#pragma region E.1 VUI syntax
	//E.1.1 VUI parameters syntax
	bool aspect_ratio_info_present_flag = false;
	uint32_t aspect_ratio_idc = 0;
	uint32_t sar_width = 0;
	uint32_t sar_height = 0;
	bool overscan_info_present_flag = false;
	bool overscan_appropriate_flag = false;
	bool video_signal_type_present_flag = false;
	uint32_t video_format = 0;
	bool video_full_range_flag = false;
	bool colour_description_present_flag = false;
	uint32_t colour_primaries = 0;
	uint32_t transfer_characteristics = 0;
	uint32_t matrix_coefficients = 0;
	bool chroma_loc_info_present_flag = false;
	uint32_t chroma_sample_loc_type_top_field = 0;
	uint32_t chroma_sample_loc_type_bottom_field = 0;
	bool timing_info_present_flag = false;
	uint32_t num_units_in_tick = 0;
	uint32_t time_scale = 0;
	bool fixed_frame_rate_flag = false;
	bool nal_hrd_parameters_present_flag = false;
	bool vcl_hrd_parameters_present_flag = false;
	bool low_delay_hrd_flag = false;
	bool pic_struct_present_flag = false;
	bool bitstream_restriction_flag = false;
	bool motion_vectors_over_pic_boundaries_flag = false;
	uint32_t max_bytes_per_pic_denom = 0;
	uint32_t max_bits_per_mb_denom = 0;
	uint32_t log2_max_mv_length_horizontal = 0;
	uint32_t log2_max_mv_length_vertical = 0;
	uint32_t max_num_reorder_frames = 0;
	uint32_t max_dec_frame_buffering = 0;
#define Extended_SAR 255
	void vui_parameters() {
		aspect_ratio_info_present_flag = read_bits(1);// 0 u(1)
		if (aspect_ratio_info_present_flag) {
			aspect_ratio_idc = read_bits(8);// 0 u(8)
			if (aspect_ratio_idc == Extended_SAR) {
				sar_width = read_bits(16);// 0 u(16)
				sar_height = read_bits(16);// 0 u(16)
			}
		}
		overscan_info_present_flag = read_bits(1);// 0 u(1)
		if (overscan_info_present_flag)
			overscan_appropriate_flag = read_bits(1);// 0 u(1)
		video_signal_type_present_flag = read_bits(1);// 0 u(1)
		if (video_signal_type_present_flag) {
			video_format = read_bits(3);// 0 u(3)
			video_full_range_flag = read_bits(1);// 0 u(1)
			colour_description_present_flag = read_bits(1);// 0 u(1)
			if (colour_description_present_flag) {
				colour_primaries = read_bits(8);// 0 u(8)
				transfer_characteristics = read_bits(8);// 0 u(8)
				matrix_coefficients = read_bits(8);// 0 u(8)
			}
		}
		chroma_loc_info_present_flag = read_bits(1);// 0 u(1)
		if (chroma_loc_info_present_flag) {
			chroma_sample_loc_type_top_field = ue();// 0 ue(v)
			chroma_sample_loc_type_bottom_field = ue();// 0 ue(v)
		}
		timing_info_present_flag = read_bits(1);// 0 u(1)
		if (timing_info_present_flag) {
			num_units_in_tick = read_bits(32);// 0 u(32)
			time_scale = read_bits(32);// 0 u(32)
			fixed_frame_rate_flag = read_bits(1);// 0 u(1)
		}
		nal_hrd_parameters_present_flag = read_bits(1);// 0 u(1)
		if (nal_hrd_parameters_present_flag)
			hrd_parameters();// 0
		vcl_hrd_parameters_present_flag = read_bits(1);// 0 u(1)
		if (vcl_hrd_parameters_present_flag)
			hrd_parameters();// 0
		if (nal_hrd_parameters_present_flag || vcl_hrd_parameters_present_flag)
			low_delay_hrd_flag = read_bits(1);// u(1)
		pic_struct_present_flag = read_bits(1);// 0 u(1)
		bitstream_restriction_flag = read_bits(1);// 0 u(1)
		if (bitstream_restriction_flag) {
			motion_vectors_over_pic_boundaries_flag = read_bits(1);// 0 u(1)
			max_bytes_per_pic_denom = ue();// 0 ue(v)
			max_bits_per_mb_denom = ue();// 0 ue(v)
			log2_max_mv_length_horizontal = ue();// 0 ue(v)
			log2_max_mv_length_vertical = ue();// 0 ue(v)
			max_num_reorder_frames = ue();// 0 ue(v)
			max_dec_frame_buffering = ue();// 0 ue(v)
		}
	}
	// E.1.2 HRD parameters syntax
	uint32_t cpb_cnt_minus1 = 0;
	uint32_t bit_rate_scale = 0;
	uint32_t cpb_size_scale = 0;
	vector<uint32_t> bit_rate_value_minus1 = vector<uint32_t>();
	vector<uint32_t> cpb_size_value_minus1 = vector<uint32_t>();
	vector<bool> cbr_flag = vector<bool>();
	uint32_t initial_cpb_removal_delay_length_minus1 = 0;
	uint32_t cpb_removal_delay_length_minus1 = 0;
	uint32_t dpb_output_delay_length_minus1 = 0;
	uint32_t time_offset_length = 0;
	void hrd_parameters() {
		cpb_cnt_minus1 = ue();
		bit_rate_scale = read_bits(4);
		cpb_size_scale = read_bits(4);
		for (size_t SchedSelIdx = 0; SchedSelIdx <= cpb_cnt_minus1; SchedSelIdx++) {
			bit_rate_value_minus1.push_back(ue());
			cpb_size_value_minus1.push_back(ue());
			cbr_flag.push_back(read_bits(1));
		}
		initial_cpb_removal_delay_length_minus1 = read_bits(5);
		cpb_removal_delay_length_minus1 = read_bits(5);
		dpb_output_delay_length_minus1 = read_bits(5);
		time_offset_length = read_bits(5);
	}
#pragma endregion

#pragma endregion
#pragma region Annex G
	//G.7.3.2.1.4 Sequence parameter set MVC extension syntax
	//（Multiview Video Coding，MVC）
	void seq_parameter_set_mvc_extension() {}
	void seq_parameter_set_mvcd_extension() {}
	void seq_parameter_set_3davc_extension() {}
	void mvc_vui_parameters_extension() {}
	void ref_pic_list_mvc_modification() {}
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

	H264Decoder video("video/output.h264");

	return 0;
}
