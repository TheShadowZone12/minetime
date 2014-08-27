/*
Minetest
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>,
			  2012-2013 RealBadAngel, Maciej Kasatkin <mk@realbadangel.pl>
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "irr_v3d.h"
#include <stack>
#include "util/pointer.h"
#include "util/numeric.h"
#include "util/mathconstants.h"
#include "map.h"
#include "environment.h"
#include "nodedef.h"
#include "treegen.h" 
#include "cmath"

namespace treegen
{

void make_tree(ManualMapVoxelManipulator &vmanip, v3s16 p0,
		bool is_apple_tree, INodeDefManager *ndef, int seed)
{
	MapNode treenode(ndef->getId("mapgen_tree"));
	MapNode leavesnode(ndef->getId("mapgen_leaves"));
	MapNode applenode(ndef->getId("mapgen_apple"));

	s16 trunk_h = rand() % 7 + 5;
	v3s16 p1 = p0;

	VoxelArea leaves_a(v3s16(-3,-3,-3), v3s16(3,3,3));

	if(is_apple_tree){trunk_h = 5;}

	for (s16 trunk=0; trunk<=trunk_h ; trunk++)
	{
		if(vmanip.m_area.contains(p1))
			if(trunk == 0 || vmanip.getNodeNoExNoEmerge(p1).getContent() == CONTENT_AIR)
				vmanip.m_data[vmanip.m_area.index(p1)] = treenode;
		p1.Y++;

	}

	for(s16 z= -3; z<= 3; z++)
	for(s16 y= -3; y<= 3; y++)
	for(s16 x= -3; x<= 3; x++)
	{
		u32 vi = vmanip.m_area.index(v3s16(p0.X + x, p0.Y + y + trunk_h, p0.Z + z));
                if (vmanip.m_area.contains(vi) == false) {
		    //std::cout<<"line " << __LINE__ << " == false\n";
		    continue;
                }
                if (rand() % 30 > 22)
                    continue;

		if ( (std::abs(x) + std::abs(z) <= 5) && (std::abs(x) + std::abs(y) <= 5) && (std::abs(z) + std::abs(y) <= 5) ) {
			//std::cout << "all good to go\n";
			
			if(vmanip.m_data[vi].getContent() != CONTENT_AIR && vmanip.m_data[vi].getContent() != CONTENT_IGNORE  ) {
			    //std::cout << "line " << __LINE__ << " != air || ignore @ pos "<< vi << "\n";
			    continue;
                        } 

			//std::cout << "ready to place leaves || apples\n";
            
            bool is_apple = is_apple_tree && rand() % 100 < 10;
            vmanip.m_data[vi] = is_apple ? applenode : leavesnode;


		}
	}
}

// L-System tree LUA spawner
treegen::error spawn_ltree(ServerEnvironment *env, v3s16 p0, INodeDefManager *ndef, TreeDef tree_definition)
{
	ServerMap *map = &env->getServerMap();
	std::map<v3s16, MapBlock*> modified_blocks;
	ManualMapVoxelManipulator vmanip(map);
	v3s16 tree_blockp = getNodeBlockPos(p0);
	treegen::error e;

	vmanip.initialEmerge(tree_blockp - v3s16(1,1,1), tree_blockp + v3s16(1,3,1));
	e = make_ltree (vmanip, p0, ndef, tree_definition);
	if (e != SUCCESS)
		return e;

	vmanip.blitBackAll(&modified_blocks);

	// update lighting
	std::map<v3s16, MapBlock*> lighting_modified_blocks;
	lighting_modified_blocks.insert(modified_blocks.begin(), modified_blocks.end());
	map->updateLighting(lighting_modified_blocks, modified_blocks);
	// Send a MEET_OTHER event
	MapEditEvent event;
	event.type = MEET_OTHER;
	for(std::map<v3s16, MapBlock*>::iterator
		i = modified_blocks.begin();
		i != modified_blocks.end(); ++i)
	{
		event.modified_blocks.insert(i->first);
	}
	map->dispatchEvent(&event);
	return SUCCESS;
}

//L-System tree generator
treegen::error make_ltree(ManualMapVoxelManipulator &vmanip, v3s16 p0, INodeDefManager *ndef,
		TreeDef tree_definition)
{
	MapNode dirtnode(ndef->getId("mapgen_dirt"));
	int seed;
	if (tree_definition.explicit_seed)
	{
		seed = tree_definition.seed+14002;
	}
	else
	{
		seed = p0.X*2 + p0.Y*4 + p0.Z;      // use the tree position to seed PRNG
	}
	PseudoRandom ps(seed);

	// chance of inserting abcd rules
	double prop_a = 9;
	double prop_b = 8;
	double prop_c = 7;
	double prop_d = 6;

	//randomize tree growth level, minimum=2
	s16 iterations = tree_definition.iterations;
	if (tree_definition.iterations_random_level>0)
		iterations -= ps.range(0,tree_definition.iterations_random_level);
	if (iterations<2)
		iterations=2;

	s16 MAX_ANGLE_OFFSET = 5;
	double angle_in_radians = (double)tree_definition.angle*M_PI/180;
	double angleOffset_in_radians = (s16)(ps.range(0,1)%MAX_ANGLE_OFFSET)*M_PI/180;

	//initialize rotation matrix, position and stacks for branches
	core::matrix4 rotation;
	rotation = setRotationAxisRadians(rotation, M_PI/2,v3f(0,0,1));
	v3f position;
	position.X = p0.X;
	position.Y = p0.Y;
	position.Z = p0.Z;
	std::stack <core::matrix4> stack_orientation;
	std::stack <v3f> stack_position;

	//generate axiom
	std::string axiom = tree_definition.initial_axiom;
	for(s16 i=0; i<iterations; i++)
	{
		std::string temp = "";
		for(s16 j=0; j<(s16)axiom.size(); j++)
		{
			char axiom_char = axiom.at(j);
			switch (axiom_char)
			{
			case 'A':
				temp+=tree_definition.rules_a;
				break;
			case 'B':
				temp+=tree_definition.rules_b;
				break;
			case 'C':
				temp+=tree_definition.rules_c;
				break;
			case 'D':
				temp+=tree_definition.rules_d;
				break;
			case 'a':
				if (prop_a >= ps.range(1,10))
					temp+=tree_definition.rules_a;
				break;
			case 'b':
				if (prop_b >= ps.range(1,10))
					temp+=tree_definition.rules_b;
				break;
			case 'c':
				if (prop_c >= ps.range(1,10))
					temp+=tree_definition.rules_c;
				break;
			case 'd':
				if (prop_d >= ps.range(1,10))
					temp+=tree_definition.rules_d;
				break;
			default:
				temp+=axiom_char;
				break;
			}
		}
		axiom=temp;
	}

	//make sure tree is not floating in the air
	if (tree_definition.trunk_type == "double")
	{
		tree_node_placement(vmanip,v3f(position.X+1,position.Y-1,position.Z),dirtnode);
		tree_node_placement(vmanip,v3f(position.X,position.Y-1,position.Z+1),dirtnode);
		tree_node_placement(vmanip,v3f(position.X+1,position.Y-1,position.Z+1),dirtnode);
	}
	else if (tree_definition.trunk_type == "crossed")
	{
		tree_node_placement(vmanip,v3f(position.X+1,position.Y-1,position.Z),dirtnode);
		tree_node_placement(vmanip,v3f(position.X-1,position.Y-1,position.Z),dirtnode);
		tree_node_placement(vmanip,v3f(position.X,position.Y-1,position.Z+1),dirtnode);
		tree_node_placement(vmanip,v3f(position.X,position.Y-1,position.Z-1),dirtnode);
	}

	/* build tree out of generated axiom

	Key for Special L-System Symbols used in Axioms

    G  - move forward one unit with the pen up
    F  - move forward one unit with the pen down drawing trunks and branches
    f  - move forward one unit with the pen down drawing leaves (100% chance)
    T  - move forward one unit with the pen down drawing trunks only
    R  - move forward one unit with the pen down placing fruit
    A  - replace with rules set A
    B  - replace with rules set B
    C  - replace with rules set C
    D  - replace with rules set D
    a  - replace with rules set A, chance 90%
    b  - replace with rules set B, chance 80%
    c  - replace with rules set C, chance 70%
    d  - replace with rules set D, chance 60%
    +  - yaw the turtle right by angle degrees
    -  - yaw the turtle left by angle degrees
    &  - pitch the turtle down by angle degrees
    ^  - pitch the turtle up by angle degrees
    /  - roll the turtle to the right by angle degrees
    *  - roll the turtle to the left by angle degrees
    [  - save in stack current state info
    ]  - recover from stack state info

    */

	s16 x,y,z;
	for(s16 i=0; i<(s16)axiom.size(); i++)
	{
		char axiom_char = axiom.at(i);
		core::matrix4 temp_rotation;
		temp_rotation.makeIdentity();
		v3f dir;
		switch (axiom_char)
		{
		case 'G':
			dir = v3f(1,0,0);
			dir = transposeMatrix(rotation,dir);
			position+=dir;
			break;
		case 'T':
			tree_trunk_placement(vmanip,v3f(position.X,position.Y,position.Z),tree_definition);
			if (tree_definition.trunk_type == "double" && !tree_definition.thin_branches)
			{
				tree_trunk_placement(vmanip,v3f(position.X+1,position.Y,position.Z),tree_definition);
				tree_trunk_placement(vmanip,v3f(position.X,position.Y,position.Z+1),tree_definition);
				tree_trunk_placement(vmanip,v3f(position.X+1,position.Y,position.Z+1),tree_definition);
			}
			else if (tree_definition.trunk_type == "crossed" && !tree_definition.thin_branches)
			{
				tree_trunk_placement(vmanip,v3f(position.X+1,position.Y,position.Z),tree_definition);
				tree_trunk_placement(vmanip,v3f(position.X-1,position.Y,position.Z),tree_definition);
				tree_trunk_placement(vmanip,v3f(position.X,position.Y,position.Z+1),tree_definition);
				tree_trunk_placement(vmanip,v3f(position.X,position.Y,position.Z-1),tree_definition);
			}
			dir = v3f(1,0,0);
			dir = transposeMatrix(rotation,dir);
			position+=dir;
			break;
		case 'F':
			tree_trunk_placement(vmanip,v3f(position.X,position.Y,position.Z),tree_definition);
			if ((stack_orientation.empty() && tree_definition.trunk_type == "double") ||
				(!stack_orientation.empty() && tree_definition.trunk_type == "double" && !tree_definition.thin_branches))
			{
				tree_trunk_placement(vmanip,v3f(position.X+1,position.Y,position.Z),tree_definition);
				tree_trunk_placement(vmanip,v3f(position.X,position.Y,position.Z+1),tree_definition);
				tree_trunk_placement(vmanip,v3f(position.X+1,position.Y,position.Z+1),tree_definition);
			}
			else if ((stack_orientation.empty() && tree_definition.trunk_type == "crossed") ||
				(!stack_orientation.empty() && tree_definition.trunk_type == "crossed" && !tree_definition.thin_branches))
			{
				tree_trunk_placement(vmanip,v3f(position.X+1,position.Y,position.Z),tree_definition);
				tree_trunk_placement(vmanip,v3f(position.X-1,position.Y,position.Z),tree_definition);
				tree_trunk_placement(vmanip,v3f(position.X,position.Y,position.Z+1),tree_definition);
				tree_trunk_placement(vmanip,v3f(position.X,position.Y,position.Z-1),tree_definition);
			}
			if (stack_orientation.empty() == false)
			{
				s16 size = 1;
				for(x=-size; x<=size; x++)
					for(y=-size; y<=size; y++)
						for(z=-size; z<=size; z++)
							if (abs(x) == size && abs(y) == size && abs(z) == size)
							{
								tree_leaves_placement(vmanip,v3f(position.X+x+1,position.Y+y,position.Z+z),ps.next(), tree_definition);
								tree_leaves_placement(vmanip,v3f(position.X+x-1,position.Y+y,position.Z+z),ps.next(), tree_definition);
								tree_leaves_placement(vmanip,v3f(position.X+x,position.Y+y,position.Z+z+1),ps.next(), tree_definition);
								tree_leaves_placement(vmanip,v3f(position.X+x,position.Y+y,position.Z+z-1),ps.next(), tree_definition);
							}
			}
			dir = v3f(1,0,0);
			dir = transposeMatrix(rotation,dir);
			position+=dir;
			break;
		case 'f':
			tree_single_leaves_placement(vmanip,v3f(position.X,position.Y,position.Z),ps.next() ,tree_definition);
			dir = v3f(1,0,0);
			dir = transposeMatrix(rotation,dir);
			position+=dir;
			break;
		case 'R':
			tree_fruit_placement(vmanip,v3f(position.X,position.Y,position.Z),tree_definition);
			dir = v3f(1,0,0);
			dir = transposeMatrix(rotation,dir);
			position+=dir;
			break;

		// turtle orientation commands
		case '[':
			stack_orientation.push(rotation);
			stack_position.push(position);
			break;
		case ']':
			if (stack_orientation.empty())
				return UNBALANCED_BRACKETS;
			rotation=stack_orientation.top();
			stack_orientation.pop();
			position=stack_position.top();
			stack_position.pop();
			break;
		case '+':
			temp_rotation.makeIdentity();
			temp_rotation=setRotationAxisRadians(temp_rotation, angle_in_radians+angleOffset_in_radians,v3f(0,0,1));
			rotation*=temp_rotation;
			break;
		case '-':
			temp_rotation.makeIdentity();
			temp_rotation=setRotationAxisRadians(temp_rotation, angle_in_radians+angleOffset_in_radians,v3f(0,0,-1));
			rotation*=temp_rotation;
			break;
		case '&':
			temp_rotation.makeIdentity();
			temp_rotation=setRotationAxisRadians(temp_rotation, angle_in_radians+angleOffset_in_radians,v3f(0,1,0));
			rotation*=temp_rotation;
			break;
		case '^':
			temp_rotation.makeIdentity();
			temp_rotation=setRotationAxisRadians(temp_rotation, angle_in_radians+angleOffset_in_radians,v3f(0,-1,0));
			rotation*=temp_rotation;
			break;
		case '*':
			temp_rotation.makeIdentity();
			temp_rotation=setRotationAxisRadians(temp_rotation, angle_in_radians,v3f(1,0,0));
			rotation*=temp_rotation;
			break;
		case '/':
			temp_rotation.makeIdentity();
			temp_rotation=setRotationAxisRadians(temp_rotation, angle_in_radians,v3f(-1,0,0));
			rotation*=temp_rotation;
			break;
		default:
			break;
		}
	}

	return SUCCESS;
}

void tree_node_placement(ManualMapVoxelManipulator &vmanip, v3f p0,
		MapNode node)
{
	v3s16 p1 = v3s16(myround(p0.X),myround(p0.Y),myround(p0.Z));
	if(vmanip.m_area.contains(p1) == false)
		return;
	u32 vi = vmanip.m_area.index(p1);
	if(vmanip.m_data[vi].getContent() != CONTENT_AIR
			&& vmanip.m_data[vi].getContent() != CONTENT_IGNORE)
		return;
	vmanip.m_data[vmanip.m_area.index(p1)] = node;
}

void tree_trunk_placement(ManualMapVoxelManipulator &vmanip, v3f p0,
		TreeDef &tree_definition)
{
	v3s16 p1 = v3s16(myround(p0.X),myround(p0.Y),myround(p0.Z));
	if(vmanip.m_area.contains(p1) == false)
		return;
	u32 vi = vmanip.m_area.index(p1);
	if(vmanip.m_data[vi].getContent() != CONTENT_AIR
			&& vmanip.m_data[vi].getContent() != CONTENT_IGNORE)
		return;
	vmanip.m_data[vmanip.m_area.index(p1)] = tree_definition.trunknode;
}

void tree_leaves_placement(ManualMapVoxelManipulator &vmanip, v3f p0,
		PseudoRandom ps ,TreeDef &tree_definition)
{
	MapNode leavesnode=tree_definition.leavesnode;
	if (ps.range(1,100) > 100-tree_definition.leaves2_chance)
		leavesnode=tree_definition.leaves2node;
	v3s16 p1 = v3s16(myround(p0.X),myround(p0.Y),myround(p0.Z));
	if(vmanip.m_area.contains(p1) == false)
		return;
	u32 vi = vmanip.m_area.index(p1);
	if(vmanip.m_data[vi].getContent() != CONTENT_AIR
			&& vmanip.m_data[vi].getContent() != CONTENT_IGNORE)
		return;	
	if (tree_definition.fruit_chance>0)
	{
		if (ps.range(1,100) > 100-tree_definition.fruit_chance)
			vmanip.m_data[vmanip.m_area.index(p1)] = tree_definition.fruitnode;
		else
			vmanip.m_data[vmanip.m_area.index(p1)] = leavesnode;
	}
	else if (ps.range(1,100) > 20)
		vmanip.m_data[vmanip.m_area.index(p1)] = leavesnode;
}

void tree_single_leaves_placement(ManualMapVoxelManipulator &vmanip, v3f p0,
		PseudoRandom ps, TreeDef &tree_definition)
{
	MapNode leavesnode=tree_definition.leavesnode;
	if (ps.range(1,100) > 100-tree_definition.leaves2_chance)
		leavesnode=tree_definition.leaves2node;
	v3s16 p1 = v3s16(myround(p0.X),myround(p0.Y),myround(p0.Z));
	if(vmanip.m_area.contains(p1) == false)
		return;
	u32 vi = vmanip.m_area.index(p1);
	if(vmanip.m_data[vi].getContent() != CONTENT_AIR
		&& vmanip.m_data[vi].getContent() != CONTENT_IGNORE)
		return;
	vmanip.m_data[vmanip.m_area.index(p1)] = leavesnode;
}

void tree_fruit_placement(ManualMapVoxelManipulator &vmanip, v3f p0,
		TreeDef &tree_definition)
{
	v3s16 p1 = v3s16(myround(p0.X),myround(p0.Y),myround(p0.Z));
	if(vmanip.m_area.contains(p1) == false)
		return;
	u32 vi = vmanip.m_area.index(p1);
	if(vmanip.m_data[vi].getContent() != CONTENT_AIR
		&& vmanip.m_data[vi].getContent() != CONTENT_IGNORE)
		return;
	vmanip.m_data[vmanip.m_area.index(p1)] = tree_definition.fruitnode;
}

irr::core::matrix4 setRotationAxisRadians(irr::core::matrix4 M, double angle, v3f axis)
{
	double c = cos(angle);
	double s = sin(angle);
	double t = 1.0 - c;

	double tx  = t * axis.X;
	double ty  = t * axis.Y;
	double tz  = t * axis.Z;
	double sx  = s * axis.X;
	double sy  = s * axis.Y;
	double sz  = s * axis.Z;

	M[0] = tx * axis.X + c;
	M[1] = tx * axis.Y + sz;
	M[2] = tx * axis.Z - sy;

	M[4] = ty * axis.X - sz;
	M[5] = ty * axis.Y + c;
	M[6] = ty * axis.Z + sx;

	M[8]  = tz * axis.X + sy;
	M[9]  = tz * axis.Y - sx;
	M[10] = tz * axis.Z + c;
	return M;
}

v3f transposeMatrix(irr::core::matrix4 M, v3f v)
{
	v3f translated;
	double x = M[0] * v.X + M[4] * v.Y + M[8]  * v.Z +M[12];
	double y = M[1] * v.X + M[5] * v.Y + M[9]  * v.Z +M[13];
	double z = M[2] * v.X + M[6] * v.Y + M[10] * v.Z +M[14];
	translated.X=x;
	translated.Y=y;
	translated.Z=z;
	return translated;
}

void make_jungletree(VoxelManipulator &vmanip, v3s16 p0,
		INodeDefManager *ndef, int seed)
{
	
	MapNode jsaplingnode(ndef->getId("mapgen_jsapling"));
	vmanip.m_data[vmanip.m_area.index(p0)] = jsaplingnode;

}

}; // namespace treegen
