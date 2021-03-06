#pragma once

#include <vector>
#include <memory>
#include "cudpp\CudaDiagBlockMatrix.h"
#include "cudpp\CudaBsrMatrix.h"
#include "ldpMat\ldp_basic_mat.h"
#include "cudpp\Cuda3DArray.h"
#include "cudpp\Cuda2DArray.h"
#include <cublas.h>
namespace arcsim
{
	class ArcSimManager;
}

class ObjMesh;
namespace ldp
{
	class ClothManager;
	class LevelSet3D;	
	class BMesh;
	class BMVert;
	class BMEdge;
	class BMFace;
	class MaterialCache;
	__device__ __host__ inline size_t vertPair_to_idx(ldp::Int2 v, int n)
	{
		return size_t(v[0]) * size_t(n) + size_t(v[1]);
	}
	__device__ __host__ inline ldp::Int2 vertPair_from_idx(size_t idx, int n)
	{
		return ldp::Int2(int(idx / n), int(idx%n));
	}
	class GpuSim
	{
	public:
		//		 c
		//		 /\
		//		/  \
		//	  a ---- b
		//		\  /
		//		 \/
		//		 d
		// edge:	 ab
		// bendEdge: cd
		// faceIdx: abc, bad
		// _idxWorld: index in world space
		// _idxTex: index in tex space
		struct EdgeData
		{
			ldp::Int4 edge_idxWorld;
			ldp::Int2 edge_idxTex[2]; // two face side tex space index
			ldp::Int2 faceIdx;
			ldp::Float2 length_sqr;
			ldp::Float2 theta_uv;	// uv direction
			float dihedral_ideal = 0.f;
			float theta_initial = 0.f;
		};
		struct FaceMaterailSpaceData // must be sizeof float4
		{
			float area = 0.f;
			float mass = 0.f;
			float stretch_mult = 0.f;
			float bend_mult = 0.f;
		};
		struct NodeMaterailSpaceData // must be sizeof float4
		{
			float area = 0.f;
			float mass = 0.f;
			float __place_holder_1 = 0.f;
			float __place_holder_2 = 0.f;
		};
		struct SimParam
		{
			float dt = 0.f;				// time step
			Float3 gravity;
			int pcg_iter = 0;			// iteration of pcg solvers
			float pcg_tol = 0.f;		// tolerance of pcg method
			float stitch_ratio = 0.f;	// for stitching edges length reduction, L -= ratio * dt
			float strecth_mult = 0.f;
			float bend_mult = 0.f;
			float stitch_stiffness = 0.f;
			float handle_stiffness = 0.f;
			float collision_stiffness = 0.f;
			float friction_stiffness = 0.f;
			float repulsion_thickness = 0.f;
			float projection_thickness = 0.f;
			bool enable_selfCollision = false;
			int selfCollision_maxGridSize = 0;

			SimParam(){ setDefault(); }
			void setDefault();
		};
	public:
		GpuSim();
		~GpuSim();

		void clear();
		void init(ClothManager* clothManager);
		void init(arcsim::ArcSimManager* arcSimManager);
		void run_one_step();
		void restart();
		void updateParam();
		void updateTopology();
		void updateStitch();
		void updateMaterial();
		void setFixPositions(int nFixed, const int* ids, const Float3* targets);
	public:
		float getFps()const{ return m_fps; }
		float getStepTime()const{ return m_simParam.dt; }
		std::string getSolverInfo()const{ return m_solverInfo; }
		ObjMesh& getResultClothMesh();
		void getResultClothPieces();	//only valid for cloth manager init.
		const std::vector<Float2>& getVertTexCoords()const{ return m_texCoord_init_h; }
		const std::vector<Float3>& getCurrentVertPositions()const{ return m_x_h; }
		const std::vector<Float3>& getInitVertPositions()const{ return m_x_init_h; }
		const std::vector<Int4>& getFaceIndices()const{ return m_faces_idxWorld_h; }
		const std::vector<int>& getVertMergeIdxMap()const{ return m_vertMerge_in_out_idxMap_h; }
		void setCurrentVertPositions(const std::vector<Float3>& X);
		void setInitVertPositions(const std::vector<Float3>& X);
	protected:
		// update the whole system based on the current changes
		void updateSystem();

		// parameters
		void initParam();

		// body level set
		void initLevelSet();

		// basic topology
		void initFaceEdgeVertArray();
		void initFaceEdgeVertArray_arcSim();
		void initFaceEdgeVertArray_clothManager();
		void initBMesh();

		// stitching
		void buildStitch();
		void buildStitchVertPairs();
		void buildStitchEdges();

		// sparse system setup
		void setup_sparse_structure();

		// material
		void updateMaterialDataToFaceNode();

		// solving related
		void updateNumeric();
		void linearSolve();
		void linearBodyCollision();
		void linearSelfCollision();
		void collisionSolve();
		void update_x_v_by_dv();
		void project_outside();

		// exporting related
		void exportResultClothToObjMesh();
	protected:
		void bindTextures();
		BMEdge* findEdge(int v1, int v2); // edge with end point v1,v2
	private:
		ClothManager* m_clothManager = nullptr;
		arcsim::ArcSimManager* m_arcSimManager = nullptr;
		cusparseHandle_t m_cusparseHandle = nullptr;
		cublasHandle_t m_cublasHandle = nullptr;
		SimParam m_simParam;
		float m_fps = 0.f;
		float m_curSimulationTime = 0.f;
		float m_curStitchRatio = 0.f;
		std::string m_solverInfo;
		ldp::LevelSet3D* m_bodyLvSet_h = nullptr;
		Cuda3DArray<float> m_bodyLvSet_d;
		std::shared_ptr<ObjMesh> m_resultClothMesh;

		bool m_shouldTopologyUpdate = false;
		bool m_shouldLevelsetUpdate = false;
		bool m_shouldMaterialUpdate = false;
		bool m_shouldStitchUpdate = false;
		bool m_shouldSparseStructureUpdate = false;
		bool m_shouldRestart = false;
		bool m_shouldExportMesh = false;
		void updateDependency();
		void resetDependency(bool on);
		///////////////// mesh structure related /////////////////////////////////////////////////////////////
		std::shared_ptr<BMesh> m_bmesh;
		std::vector<BMVert*> m_bmVerts;
		std::vector<ldp::Int4> m_faces_idxWorld_h;				// the last value is its cloth index
		DeviceArray<ldp::Int4> m_faces_idxWorld_d;				
		std::vector<ldp::Int4> m_faces_idxTex_h;				// the last value is its vert start index
		DeviceArray<ldp::Int4> m_faces_idxTex_d;
		std::vector<std::string> m_faces_matName_h;				// material name of each face, ONLY used for arcsim
		DeviceArray<cudaTextureObject_t> m_faces_texStretch_d;	// cuda texture of stretching
		DeviceArray<cudaTextureObject_t> m_faces_texBend_d;		// cuda texture of bending
		DeviceArray<FaceMaterailSpaceData> m_faces_materialSpace_d;
		DeviceArray<NodeMaterailSpaceData> m_nodes_materialSpace_d;
		std::vector<EdgeData> m_edgeData_h;
		DeviceArray<EdgeData> m_edgeData_d;
		std::shared_ptr<CudaBsrMatrix> m_vert_FaceList_d;
		std::vector<std::pair<Int2, float>> m_stitch_vertPairs_h;
		std::shared_ptr<CudaBsrMatrix> m_stitch_vertPairs_d;
		std::vector<int> m_stitch_vertMerge_idxMap_h;
		std::vector<int> m_vertMerge_in_out_idxMap_h;
		std::vector<EdgeData> m_stitch_edgeData_h;
		DeviceArray<EdgeData> m_stitch_edgeData_d;
		std::shared_ptr<MaterialCache> m_materials;
		///////////////// precomputed data /////////////////////////////////////////////////////////////	
		DeviceArray<size_t> m_A_Ids_d;			// for sparse matrix, encode the (row, col) pairs, sorted
		DeviceArray<size_t> m_A_Ids_d_unique;
		DeviceArray<int> m_A_Ids_d_unique_pos;	// the array position kept after unique
		DeviceArray<int> m_A_Ids_start_d;		// the starting position of A_order
		DeviceArray<int> m_A_order_d;			// the face/edge/bend filling order, beforScan_A[order]
		DeviceArray<int> m_A_invOrder_d;
		DeviceArray<ldp::Mat3f> m_beforScan_A;
		DeviceArray<int> m_b_Ids_d;				// for rhs construction
		DeviceArray<int> m_b_Ids_d_unique;
		DeviceArray<int> m_b_Ids_d_unique_pos;
		DeviceArray<int> m_b_Ids_start_d;		// the starting position of b_order
		DeviceArray<int> m_b_order_d;			// the face/edge/bend filling order, beforScan_b[order]
		DeviceArray<int> m_b_invOrder_d;
		DeviceArray<ldp::Float3> m_beforScan_b;
		///////////////// solve for the simulation linear system: A*dv=b////////////////////////////////
		std::shared_ptr<CudaBsrMatrix> m_A_d;
		std::shared_ptr<CudaDiagBlockMatrix> m_A_diag_d;
		DeviceArray<ldp::Float3> m_b_d;
		std::vector<ldp::Float2> m_texCoord_init_h;				// material (tex) space vertex texCoord							
		DeviceArray<ldp::Float2> m_texCoord_init_d;				// material (tex) space vertex texCoord	
		std::vector<ldp::Float3> m_x_init_h;					// world space vertex position	
		DeviceArray<ldp::Float3> m_x_init_d;					// world space vertex position
		std::vector<ldp::Float3> m_x_h;							// position of current step	
		DeviceArray<ldp::Float3> m_x_d;							// position of current step	
		DeviceArray<ldp::Float3> m_last_x_d;					// position of last step	
		DeviceArray<ldp::Float3> m_v_d;							// velocity of current step
		DeviceArray<ldp::Float3> m_last_v_d;					// velocity of last step	
		DeviceArray<ldp::Float3> m_dv_d;						// velocity changed in this step
		DeviceArray<ldp::Float4> m_project_vw_d;
		std::vector<ldp::Float4> m_fixPosition_vw_h;
		DeviceArray<ldp::Float4> m_fixPosition_vw_d;
		//////////////////////// self collision related///////////////////////////////////////////////////////
		int m_selfColli_nBuckets = 0;
		DeviceArray<int> m_selfColli_vertIds;
		DeviceArray<int> m_selfColli_bucketIds;
		DeviceArray<int> m_selfColli_bucketRanges;
		DeviceArray<int> m_selfColli_tri_vertCnt;
		DeviceArray<int> m_selfColli_tri_vertPair_tId;
		DeviceArray<int> m_selfColli_tri_vertPair_vId;
		int m_nPairs = 0;
		int m_debug_flag = 0;
	protected:
		//////////////////////// pcg related///////////////////////////////////////////////////////
		void pcg_vecMul(int n, const float* a_d, const float* b_d, 
			float* c_d, float alpha = 1.f, float beta = 0.f)const; // c=alpha * a * b + beta
		void pcg_update_p(int n, const float* z_d, float* p_d, float* pcg_orz_rz_pAp)const;
		void pcg_update_x_r(int n, const float* p_d, const float* Ap_d, float* x_d, 
			float* r_d, float* pcg_orz_rz_pAp)const;
		void pcg_dot_rz(int n, const float* a_d, const float* b_d, float* pcg_orz_rz_pAp)const;
		void pcg_dot_pAp(int n, const float* a_d, const float* b_d, float* pcg_orz_rz_pAp)const;
		void pcg_extractInvDiagBlock(const CudaBsrMatrix& A, CudaDiagBlockMatrix& invD);
		//////////////////////// helper related///////////////////////////////////////////////////////
		static void vertPair_to_idx(const int* ids_v1, const int* ids_v2, size_t* ids, int nVerts, int nPairs);
		static void vertPair_from_idx(int* ids_v1, int* ids_v2, const size_t* ids, int nVerts, int nPairs);
		static void dumpVec(std::string name, const DeviceArray2D<float>& A);
		static void dumpVec(std::string name, const DeviceArray<float>& A, int nTotal=-1);
		static void dumpVec(std::string name, const DeviceArray<ldp::Float3>& A, int nTotal = -1);
		static void dumpVec(std::string name, const DeviceArray<ldp::Mat3f>& A, int nTotal = -1);
		static void dumpVec(std::string name, const DeviceArray<ldp::Float2>& A, int nTotal = -1);
		static void dumpVec(std::string name, const DeviceArray<ldp::Int2>& A, int nTotal = -1);
		static void dumpVec(std::string name, const DeviceArray<int>& A, int nTotal = -1);
		static void dumpVec_pair(std::string name, const DeviceArray<size_t>& A, int nVerts, int nTotal = -1);
		static void dumpEdgeData(std::string name, std::vector<EdgeData>& eDatas);
	};
}