/* --------------------------------------------------------------------
EXTREME TUXRACER

Copyright (C) 1999-2001 Jasmin F. Patry (Tuxracer)
Copyright (C) 2010 Extreme Tux Racer Team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
---------------------------------------------------------------------*/
// PSP port modifications, 2026-09-07. See docs/porting.md in the port repository.


#ifndef TUX_H
#define TUX_H

#include "bh.h"
#include "mathlib.h"
#include <unordered_map>
#include <vector>

#define MAX_ACTIONS 8
#define	MAX_CHAR_NODES 256

#define MIN_SPHERE_DIV 3
#define MAX_SPHERE_DIV 16

struct TCharMaterial {
	sf::Color diffuse;
	sf::Color specular;
	float exp;
	std::string matline;
};

struct TCharAction {
	std::size_t num;
	int type[MAX_ACTIONS];
	TVector3d vec[MAX_ACTIONS];
	float dval[MAX_ACTIONS];
	std::string name;
	std::string order;
	std::string mat;
};

struct TCharNode {
	TCharNode *parent;
	TCharNode *next;
	TCharNode *child;

	TCharAction* action;

	std::size_t node_idx;	// number in node_array
	std::size_t node_name;	// int identifier of node itself
	std::size_t parent_name;	// int identifier of parent
	std::size_t child_name;
	std::size_t next_name;

	std::string joint;
	TMatrix<4, 4> trans;
	TMatrix<4, 4> invtrans;
	float radius;
	TCharMaterial *mat;
	int divisions;
	bool render_shadow;
	bool visible;
};

class CCharShape {
private:
	TCharNode *Nodes[MAX_CHAR_NODES];
	std::size_t Index[MAX_CHAR_NODES];
	std::size_t numNodes;
	std::vector<TCharMaterial> Materials;
	std::unordered_map<std::string, std::size_t> MaterialIndex;
	bool useActions;
	bool newActions;

	// nodes
	std::size_t GetNodeIdx(std::size_t node_name) const;
	TCharNode *GetNode(std::size_t node_name) const;
	void CreateRootNode();
	bool CreateCharNode(int parent_name, std::size_t node_name, const std::string& joint,
	                    const std::string& name, const std::string& order, bool shadow);
	bool VisibleNode(std::size_t node_name, float level);
	bool MaterialNode(std::size_t node_name, const std::string& mat_name);
	bool TransformNode(std::size_t node_name, const TMatrix<4, 4>& mat, const TMatrix<4, 4>& invmat);

	// material
	TCharMaterial* GetMaterial(const std::string& mat_name);
	void CreateMaterial(const std::string& line);

	// drawing
	void DrawCharSphere(int num_divisions) const;
	void DrawNodes(const TCharNode *node);
	TVector3d AdjustRollvector(const CControl *ctrl, const TVector3d& vel, const TVector3d& zvec);

	// collision
	bool CheckPolyhedronCollision(const TCharNode *node, const TMatrix<4, 4>& modelMatrix,
	                              const TMatrix<4, 4>& invModelMatrix, const TPolyhedron& ph);
	bool CheckCollision(const TPolyhedron& ph);

	// shadow
	void DrawShadowVertex(float x, float y, float z, const TMatrix<4, 4>& mat) const;
	void DrawShadowSphere(const TMatrix<4, 4>& mat) const;
	void TraverseDagForShadow(const TCharNode *node, const TMatrix<4, 4>& mat) const;

	// testing and developing
	void AddAction(std::size_t node_name, int type, const TVector3d& vec, float val);
public:
	CCharShape();
	~CCharShape();
	bool useMaterials;
	bool useHighlighting;
	bool   highlighted;
	std::size_t highlight_node;
	std::unordered_map<std::string, std::size_t> NodeIndex;

	// nodes
	bool ResetNode(std::size_t node_name);
	bool ResetNode(const std::string& node_trivialname);
	bool TranslateNode(std::size_t node_name, const TVector3d& vec);
	bool RotateNode(std::size_t node_name, int axis, float angle);
	bool RotateNode(const std::string& node_trivialname, int axis, float angle);
	void ScaleNode(std::size_t node_name, const TVector3d& vec);
	void ResetRoot() { ResetNode(0); }
	void ResetJoints();

	// global functions
	void Reset();
	void Draw();
	void DrawShadow() const;
	bool Load(const std::string& dir, const std::string& filename, bool with_actions);

	void AdjustOrientation(CControl *ctrl, float dtime,
	                       float dist_from_surface, const TVector3d& surf_nml);
	void AdjustJoints(float turnFact, bool isBraking,
	                  float paddling_factor, float speed,
	                  const TVector3d& net_force, float flap_factor);
	bool Collision(const TVector3d& pos, const TPolyhedron& ph);

	std::size_t GetNodeName(std::size_t idx) const;
	std::size_t GetNodeName(const std::string& node_trivialname) const;
	std::string GetNodeJoint(std::size_t idx) const;
	std::size_t GetNumNodes() const { return numNodes; }
	const std::string& GetNodeFullname(std::size_t idx) const;
	std::size_t GetNumActs(std::size_t idx) const;
	TCharAction *GetAction(std::size_t idx) const;
	void   PrintAction(std::size_t idx) const;
	void   PrintNode(std::size_t idx) const;
	void   RefreshNode(std::size_t idx);
	void   SaveCharNodes(const std::string& dir, const std::string& filename);
};

// only for char tools, the characters for playing are in
// CCharacter (game_ctrl)
extern CCharShape TestChar;

#endif
