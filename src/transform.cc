/*
 *  OpenSCAD (www.openscad.org)
 *  Copyright (C) 2009-2011 Clifford Wolf <clifford@clifford.at> and
 *                          Marius Kintel <marius@kintel.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  As a special exception, you have permission to link this program
 *  with the CGAL library and distribute executables, as long as you
 *  follow the requirements of the GNU GPL in regard to all of the
 *  software in the executable aside from CGAL.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "transformnode.h"
#include "ModuleInstantiation.h"
#include "evalcontext.h"
#include "polyset.h"
#include "builtin.h"
#include "value.h"
#include "printutils.h"
#include "degree_trig.h"
#include <sstream>
#include <vector>
#include <assert.h>
#include <boost/assign/std/vector.hpp>
#include "boost-utils.h"
using namespace boost::assign; // bring 'operator+=()' into scope

enum class transform_type_e {
	SCALE,
	ROTATE,
	MIRROR,
	TRANSLATE,
	MULTMATRIX
};

class TransformModule : public AbstractModule
{
public:
	transform_type_e type;
	TransformModule(transform_type_e type) : type(type) { }
	AbstractNode *instantiate(const std::shared_ptr<Context>& ctx, const ModuleInstantiation *inst, const std::shared_ptr<EvalContext>& evalctx) const override;
};

AbstractNode *TransformModule::instantiate(const std::shared_ptr<Context>& ctx, const ModuleInstantiation *inst, const std::shared_ptr<EvalContext>& evalctx) const
{
	auto node = new TransformNode(inst, evalctx);

	AssignmentList args;

	switch (this->type) {
	case transform_type_e::SCALE:
		args += assignment("v");
		break;
	case transform_type_e::ROTATE:
		args += assignment("a"), assignment("v");
		break;
	case transform_type_e::MIRROR:
		args += assignment("v");
		break;
	case transform_type_e::TRANSLATE:
		args += assignment("v");
		break;
	case transform_type_e::MULTMATRIX:
		args += assignment("m");
		break;
	default:
		assert(false);
	}

	ContextHandle<Context> c{Context::create<Context>(ctx)};
	c->setVariables(evalctx, args);
	inst->scope.apply(evalctx);

	if (this->type == transform_type_e::SCALE) {
		Vector3d scalevec(1, 1, 1);
		auto v = c->lookup_variable("v");
		if (!v->getVec3(scalevec[0], scalevec[1], scalevec[2], 1.0)) {
			double num;
			if (v->getDouble(num)){
				scalevec.setConstant(num);
			}else{
				LOG(message_group::Warning,inst->location(),ctx->documentPath(),"Unable to convert scale(%1$s) parameter to a number, a vec3 or vec2 of numbers or a number",v->toEchoString());
			}
		}
		if(OpenSCAD::rangeCheck){
			if(scalevec[0]==0 || scalevec[1]==0 || scalevec[2]==0 || !std::isfinite(scalevec[0])|| !std::isfinite(scalevec[1])|| !std::isfinite(scalevec[2])){
				LOG(message_group::Warning,inst->location(),ctx->documentPath(),"scale(%1$s)",v->toEchoString());
			}
		}
		node->matrix.scale(scalevec);
	}
	else if (this->type == transform_type_e::ROTATE) {
		auto val_a = c->lookup_variable("a");
		auto val_v = c->lookup_variable("v");
		if (val_a->type() == Value::Type::VECTOR) {
			double sx = 0, sy = 0, sz = 0;
			double cx = 1, cy = 1, cz = 1;
			double a = 0.0;
			bool ok = true;
			if (val_a->toVector().size() > 0) {
				ok &= val_a->toVector()[0]->getDouble(a);
				ok &= !std::isinf(a) && !std::isnan(a);
				sx = sin_degrees(a);
				cx = cos_degrees(a);
			}
			if (val_a->toVector().size() > 1) {
				ok &= val_a->toVector()[1]->getDouble(a);
				ok &= !std::isinf(a) && !std::isnan(a);
				sy = sin_degrees(a);
				cy = cos_degrees(a);
			}
			if (val_a->toVector().size() > 2) {
				ok &= val_a->toVector()[2]->getDouble(a);
				ok &= !std::isinf(a) && !std::isnan(a);
				sz = sin_degrees(a);
				cz = cos_degrees(a);
			}
			if (val_a->toVector().size() > 3) {
				ok &= false;
			}

			bool v_supplied = (val_v->isDefined());
			if(ok){
				if(v_supplied){
					LOG(message_group::Warning,inst->location(),ctx->documentPath(),"When parameter a is supplied as vector, v is ignored rotate(a=%1$s, v=%2$s)",val_a->toEchoString(),val_v->toEchoString());
				}
			}else{
				if(v_supplied){
					LOG(message_group::Warning,inst->location(),ctx->documentPath(),"Problem converting rotate(a=%1$s, v=%2$s) parameter",val_a->toEchoString(),val_v->toEchoString());
				}else{
					LOG(message_group::Warning,inst->location(),ctx->documentPath(),"Problem converting rotate(a=%1$s) parameter",val_a->toEchoString());
				}
			}
			Matrix3d M;
			M <<  cy * cz,  cz * sx * sy - cx * sz,   cx * cz * sy + sx * sz,
			      cy * sz,  cx * cz + sx * sy * sz,  -cz * sx + cx * sy * sz,
			     -sy,       cy * sx,                  cx * cy;
			node->matrix.rotate(M);
		} else {
			double a = 0.0;
			bool aConverted = val_a->getDouble(a);
			aConverted &= !std::isinf(a) && !std::isnan(a);

			Vector3d v(0, 0, 1);
			bool vConverted = val_v->getVec3(v[0], v[1], v[2], 0.0);
			node->matrix.rotate(angle_axis_degrees(aConverted ? a : 0, v));
			if(val_v->isDefined() && ! vConverted){
				if(aConverted){
					LOG(message_group::Warning,inst->location(),ctx->documentPath(),
						"Problem converting rotate(..., v=%1$s) parameter",val_v->toEchoString());
				}else{
					LOG(message_group::Warning,inst->location(),ctx->documentPath(),
						"Problem converting rotate(a=%1$s, v=%2$s) parameter",val_a->toEchoString(),val_v->toEchoString());
				}
			}else if(!aConverted){
					LOG(message_group::Warning,inst->location(),ctx->documentPath(),
						"Problem converting rotate(a=%1$s) parameter",val_a->toEchoString());
			}
		}
	}
	else if (this->type == transform_type_e::MIRROR) {
		auto val_v = c->lookup_variable("v");
		double x = 1.0, y = 0.0, z = 0.0;

		if (!val_v->getVec3(x, y, z, 0.0)) {
			LOG(message_group::Warning,inst->location(),ctx->documentPath(),
				"Unable to convert mirror(%1$s) parameter to a vec3 or vec2 of numbers",val_v->toEchoString());
		}

		// x /= sqrt(x*x + y*y + z*z)
		// y /= sqrt(x*x + y*y + z*z)
		// z /= sqrt(x*x + y*y + z*z)
		if (x != 0.0 || y != 0.0 || z != 0.0)	{
			// skip using sqrt to normalize the vector since each element of matrix contributes it with two multiplied terms
			// instead just divide directly within each matrix element
			// simplified calculation leads to less float errors
			double a = x*x + y*y + z*z;

			Matrix4d m;
			m << 1-2*x*x/a, -2*y*x/a, -2*z*x/a, 0,
				-2*x*y/a, 1-2*y*y/a, -2*z*y/a, 0,
				-2*x*z/a, -2*y*z/a, 1-2*z*z/a, 0,
				0, 0, 0, 1;
			node->matrix = m;
		}
	}
	else if (this->type == transform_type_e::TRANSLATE)	{
		auto v = c->lookup_variable("v");
		Vector3d translatevec(0,0,0);
		bool ok = v->getVec3(translatevec[0], translatevec[1], translatevec[2], 0.0);
		ok &= std::isfinite(translatevec[0]) && std::isfinite(translatevec[1]) && std::isfinite(translatevec[2]) ;
		if (ok) {
			node->matrix.translate(translatevec);
		}else{
			LOG(message_group::Warning,inst->location(),ctx->documentPath(),
				"Unable to convert translate(%1$s) parameter to a vec3 or vec2 of numbers",v->toEchoString());
		}
	}
	else if (this->type == transform_type_e::MULTMATRIX) {
		auto v = c->lookup_variable("m");
		if (v->type() == Value::Type::VECTOR) {
			Matrix4d rawmatrix{Matrix4d::Identity()};
			for (int i = 0; i < 16; ++i) {
				size_t x = i / 4, y = i % 4;
				if (y < v->toVector().size() && v->toVector()[y]->type() ==
						Value::Type::VECTOR && x < v->toVector()[y]->toVector().size())
					v->toVector()[y]->toVector()[x]->getDouble(rawmatrix(y, x));
			}
			double w = rawmatrix(3,3);
			if (w != 1.0) node->matrix = rawmatrix / w;
			else node->matrix = rawmatrix;
		}
	}

	auto instantiatednodes = inst->instantiateChildren(evalctx);
	node->children.insert(node->children.end(), instantiatednodes.begin(), instantiatednodes.end());

	return node;
}

std::string TransformNode::toString() const
{
	std::ostringstream stream;

	stream << "multmatrix([";
	for (int j=0; j<4; ++j) {
		stream << "[";
		for (int i=0; i<4; ++i) {
			Value v(this->matrix(j, i));
			stream << v;
			if (i != 3) stream << ", ";
		}
		stream << "]";
		if (j != 3) stream << ", ";
	}
	stream << "])";

	return stream.str();
}

TransformNode::TransformNode(const ModuleInstantiation *mi, const std::shared_ptr<EvalContext> &ctx) : AbstractNode(mi, ctx), matrix(Transform3d::Identity())
{
}

std::string TransformNode::name() const
{
	return "transform";
}

void register_builtin_transform()
{
	Builtins::init("scale", new TransformModule(transform_type_e::SCALE),
				{
					"scale([x, y, z])",
				});

	Builtins::init("rotate", new TransformModule(transform_type_e::ROTATE),
				{
					"rotate([x, y, z])",
				});

	Builtins::init("mirror", new TransformModule(transform_type_e::MIRROR),
				{
					"mirror([x, y, z])",
				});

	Builtins::init("translate", new TransformModule(transform_type_e::TRANSLATE),
				{
					"translate([x, y, z])",
				});

	Builtins::init("multmatrix", new TransformModule(transform_type_e::MULTMATRIX),
				{
					"multmatrix(matrix_4_by_4)",
				});
}
