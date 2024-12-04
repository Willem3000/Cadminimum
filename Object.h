#pragma once

#include <GL\glew.h>
#include <GLFW\glfw3.h>
#include <glm\glm.hpp>

#include "model.h"

struct Transform
{
	glm::vec3 position;
	glm::vec3 rotation;
	glm::vec3 scale;
};

class Object
{
public:
	Object() {}
	Object(Model& model_, std::string& name_) {
		model = model_;
		name = name_.c_str();

		setScale(1.0f, 1.0f, 1.0f);
	}
	~Object(){}

	glm::vec3 getPos() { return transform.position; }
	glm::vec3 getRot() { return transform.rotation; }
	glm::vec3 getScale() { return transform.scale; }

	void setPos(float x, float y, float z) {
		transform.position.x = x;
		transform.position.y = y;
		transform.position.z = z;
	}

	void setRot(float x, float y, float z) {
		transform.rotation.x = x;
		transform.rotation.y = y;
		transform.rotation.z = z;
	}

	void setScale(float x, float y, float z) {
		transform.scale.x = x;
		transform.scale.y = y;
		transform.scale.z = z;
	}

	Model getModel() { return model; }
	void setModel(Model model_) { model = model_; }

	bool getIsSelected() { return isSelected; }
	void setIsSelected(bool isSelected_) { isSelected = isSelected_; }

	const char* getName() { return name; }
	void setName(const char* name_) { name = name_; }

private:
	Transform transform;
	Model model;
	bool isSelected;
	const char* name;
};

