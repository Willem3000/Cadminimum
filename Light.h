#pragma once

#include <GL\glew.h>
#include <glm\glm.hpp>
#include <glm\gtc\matrix_transform.hpp>

#include "ShadowMap.h"

class Light
{
public:
	Light();
	Light(GLuint shadowWidth, GLuint shadowHeight, 
			GLfloat red, GLfloat green, GLfloat blue,
			GLfloat aIntensity, GLfloat dIntensity);

	ShadowMap* getShadowMap() { return shadowMap; }

	glm::vec3 getColour() { return colour; }
	void setColour(float colour_[3]) {
		colour.x = colour_[0];
		colour.y = colour_[1];
		colour.z = colour_[2];
	}

	GLfloat getAmbientIntensity() { return ambientIntensity; }
	void setAmbientIntensity(GLfloat intensity) { ambientIntensity = intensity; }
	GLfloat getDiffuseIntensity() { return diffuseIntensity; }
	void setDiffuseIntensity(GLfloat intensity) { diffuseIntensity = intensity; }

	~Light();

protected:
	glm::vec3 colour;
	GLfloat ambientIntensity;
	GLfloat diffuseIntensity;

	glm::mat4 lightProj;

	ShadowMap* shadowMap;
};

