#pragma once

#include "ldpMat\ldp_basic_mat.h"
#include <eigen\Dense>
#include <eigen\Sparse>
#include "Renderable\Renderable.h"

class ObjMesh;
class SmplManager : public Renderable
{
public:
	enum BsStyle{
		LBS = 0,
		DQBS
	};
	enum BsType{
		LROTMIN = 0,
	};
	typedef float real;
	typedef ldp::Int3 Face;
	typedef Eigen::SparseMatrix<real> SpMat;
	typedef Eigen::Matrix<real, -1, -1> DMat;
public:
	SmplManager();
	~SmplManager();

	virtual void render(int showType, int frameIndex = 0);
	virtual void renderConstColor(Float3 color)const {}
	virtual void renderForSelection(int showType, int idStart);
	virtual void selectAction(ldp::Float4 selectedColor, SelectActionType actionType, int actionCode);
	virtual void transformAction(TransformActionType act, ldp::Float2 mousePos);
	virtual void setAxisRenderMode(AxisRenderMode mode);
	virtual void setRenderCamera(const ldp::Camera* cam);
	virtual int getMeshType()const{ return TYPE_SMPL; }
	virtual ldp::Float3 getCenter()const { return (m_bbox[0] + m_bbox[1])*0.5f; }
	virtual ldp::Float3 getBoundingBox(int i)const{ return m_bbox[i]; }
	virtual void clear();

	bool isInitialized()const { return m_inited; }

	void loadFromMat(const char* filename);
	void toObjMesh(ObjMesh& mesh)const;
	int numShapes()const{ return m_curShapes.size(); }
	int numVarEachPose()const{ return m_curPoses.cols(); }
	int numPoses()const{ return m_curPoses.rows(); }
	void setPoseShapeVals(const std::vector<float>* poses = nullptr, const std::vector<float>* shapes = nullptr);

	int numFaces()const { return (int)m_faces.size(); }
	const Face* facePtr()const { return  m_faces.data(); }
	int numVertices()const { return (int)m_curVerts.size(); }
	const ldp::Float3* vertexPtr()const { return m_curVerts.data(); }

	void saveShapeCoeffs(std::string filename)const;
	void loadShapeCoeffs(std::string filename);
	void savePoseCoeffs(std::string filename)const;
	void loadPoseCoeffs(std::string filename);
public:
	ldp::Float3 getCurNodeCenter(int idx)const;
	ldp::Float3 getCurNodeCenterNotPosed(int idx)const;
	const ldp::Mat3f& getNodeGlobalRotation(int idx)const { return m_curJrots.at(idx); }
	const ldp::Float3& getNodeGlobalTranslation(int idx)const { return m_curJtrans.at(idx); }
	int getNodeParent(int idx)const { return m_kintree_table.at(idx); }
	float getCurShapeCoef(int idx) const { return m_curShapes(idx, 0); }
	float getCurPoseCoef(int idx, int axis) const { return m_curPoses(idx, axis); }

	// update the binded mesh based on current joints
	void updateCurMesh();

	// update node global translation/rotation based their local rotations
	void calcGlobalTrans();

	// call updateCurMesh() after set
	void setCurShapeCoef(int idx, float val) { m_curShapes(idx, 0) = val; }
	void setCurPoseCoef(int idx, int axis, float val) { m_curPoses(idx, axis) = val; }
protected:
	void calcPoseVector207(const DMat& poses_24x3, DMat& poses_207);
	void selectAction_mouseMove(int selectedId);
	void selectAction_mousePress(int selectedId);
	void selectAction_mouseRelease(int selectedId);

	void transformAction_translate_begin(ldp::Float2 mousePos);
	void transformAction_translate_move(ldp::Float2 mousePos);
	void transformAction_rotate_begin(ldp::Float2 mousePos);
	void transformAction_rotate_move(ldp::Float2 mousePos);

	void renderSelectedNode_Trans(float scale, bool indexMode);
	void renderSelectedNode_Rot(float scale, bool indexMode);
private:
	bool m_inited;

	// model data -----------------------------------

	BsStyle m_bsStyle;
	BsType m_bsType;
	std::vector<Face> m_faces;
	DMat m_J;
	SpMat m_J_regressor;
	SpMat m_J_regressor_prior;
	std::vector<int> m_kintree_table;
	DMat m_posedirs;
	DMat m_shapedirs;
	DMat m_v_template;
	std::vector<int> m_vert_sym_idxs;
	SpMat m_weights;
	SpMat m_weights_prior;

	// specific data -----------------------------------

	DMat m_curPoses;
	DMat m_curShapes;
	DMat m_curJ;
	std::vector<ldp::Mat3f> m_curJrots;
	std::vector<ldp::Float3> m_curJtrans;
	std::vector<ldp::Float3> m_curVerts;
	std::vector<ldp::Float3> m_curVNormals;
	std::vector<ldp::Float3> m_curFNormals;
	ldp::Float3 m_bbox[2];

	// ui related ---------------------------------------
	const ldp::Camera* m_renderCam;
	AxisRenderMode m_axisRenderMode;
	int m_selectedNode;
	int m_selectedNodeAxis;
	int m_selectedNode_mouseMove;
	int m_selectedNodeAxis_mouseMove;
	int m_selectIdStart;
	bool m_select_mousePressed;
	ldp::Float2 m_transform_startMousePos;
	ldp::Float3 m_transform_joint_r_kept;
};
