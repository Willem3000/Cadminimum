#define STB_IMAGE_IMPLEMENTATION

#include <stdio.h>
#include <string.h>
#include <cmath>
#include <vector>
#include <filesystem>

#include <GL\glew.h>
#include <GLFW\glfw3.h>

#include <glm\glm.hpp>
#include <glm\gtc\matrix_transform.hpp>
#include <glm\gtc\type_ptr.hpp>

#include "imgui\imgui.h"
#include "imgui\imgui_impl_glfw.h"
#include "imgui\imgui_impl_opengl3.h"

#include "CommonValues.h"

#include "Window.h"
#include "Mesh.h"
#include "Shader.h"
#include "Camera.h"
#include "Texture.h"
#include "DirectionalLight.h"
#include "PointLight.h"
#include "SpotLight.h"
#include "Material.h"
#include "Object.h"
#include "Model.h"
#include "Skybox.h"

const float toRadians = 3.14159265f / 180.0f;

GLuint uniformProjection = 0, uniformModel = 0, uniformView = 0, uniformEyePosition = 0,
uniformSpecularIntensity = 0, uniformShininess = 0,
uniformDirectionalLightTransform = 0, uniformOmniLightPos = 0, uniformFarPlane = 0;

Window mainWindow;

std::vector<Shader> shaderList;
Shader directionalShadowShader;
Shader omniShadowShader;

Camera camera;

Texture plainTexture;

DirectionalLight mainLight;
PointLight pointLights[MAX_POINT_LIGHTS];
SpotLight spotLights[MAX_SPOT_LIGHTS];

Skybox skybox;

unsigned int pointLightCount = 0;
unsigned int spotLightCount = 0;

GLfloat deltaTime = 0.0f;
GLfloat lastTime = 0.0f;

GLfloat blackhawkAngle = 0.0f;

static std::vector<Object*> objects;
// Vertex Shader
static const char* vShader = "Shaders/shader.vert";

// Fragment Shader
static const char* fShader = "Shaders/shader.frag";

void calcAverageNormals(unsigned int * indices, unsigned int indiceCount, GLfloat * vertices, unsigned int verticeCount, 
						unsigned int vLength, unsigned int normalOffset)
{
	for (size_t i = 0; i < indiceCount; i += 3)
	{
		unsigned int in0 = indices[i] * vLength;
		unsigned int in1 = indices[i + 1] * vLength;
		unsigned int in2 = indices[i + 2] * vLength;
		glm::vec3 v1(vertices[in1] - vertices[in0], vertices[in1 + 1] - vertices[in0 + 1], vertices[in1 + 2] - vertices[in0 + 2]);
		glm::vec3 v2(vertices[in2] - vertices[in0], vertices[in2 + 1] - vertices[in0 + 1], vertices[in2 + 2] - vertices[in0 + 2]);
		glm::vec3 normal = glm::cross(v1, v2);
		normal = glm::normalize(normal);
		
		in0 += normalOffset; in1 += normalOffset; in2 += normalOffset;
		vertices[in0] += normal.x; vertices[in0 + 1] += normal.y; vertices[in0 + 2] += normal.z;
		vertices[in1] += normal.x; vertices[in1 + 1] += normal.y; vertices[in1 + 2] += normal.z;
		vertices[in2] += normal.x; vertices[in2 + 1] += normal.y; vertices[in2 + 2] += normal.z;
	}

	for (size_t i = 0; i < verticeCount / vLength; i++)
	{
		unsigned int nOffset = i * vLength + normalOffset;
		glm::vec3 vec(vertices[nOffset], vertices[nOffset + 1], vertices[nOffset + 2]);
		vec = glm::normalize(vec);
		vertices[nOffset] = vec.x; vertices[nOffset + 1] = vec.y; vertices[nOffset + 2] = vec.z;
	}
}


void CreateShaders()
{
	Shader *shader1 = new Shader();
	shader1->CreateFromFiles(vShader, fShader);
	shaderList.push_back(*shader1);

	directionalShadowShader.CreateFromFiles("Shaders/directional_shadow_map.vert", "Shaders/directional_shadow_map.frag");
	omniShadowShader.CreateFromFiles("Shaders/omni_shadow_map.vert", "Shaders/omni_shadow_map.geom", "Shaders/omni_shadow_map.frag");
}

void RenderScene()
{

	for (Object* object : objects) {
		glm::mat4 model(1.0f);
		model = glm::translate(model, glm::vec3(object->getPos().x, object->getPos().y, object->getPos().z));
		model = glm::rotate(model, object->getRot().x * toRadians, glm::vec3(1, 0, 0));
		model = glm::rotate(model, object->getRot().y * toRadians, glm::vec3(0, 1, 0));
		model = glm::rotate(model, object->getRot().z * toRadians, glm::vec3(0, 0, 1));
		model = glm::scale(model, glm::vec3(object->getScale().x, object->getScale().y, object->getScale().z));
		glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model));
		object->getModel().RenderModel();
	}
	//shinyMaterial.UseMaterial(uniformSpecularIntensity, uniformShininess);
}

void DirectionalShadowMapPass(DirectionalLight* light)
{
	directionalShadowShader.UseShader();

	glViewport(0, 0, light->getShadowMap()->GetShadowWidth(), light->getShadowMap()->GetShadowHeight());

	light->getShadowMap()->Write();
	glClear(GL_DEPTH_BUFFER_BIT);

	uniformModel = directionalShadowShader.GetModelLocation();
	//directionalShadowShader.SetDirectionalLightTransform(&light->CalculateLightTransform());

	directionalShadowShader.Validate();

	RenderScene();

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void OmniShadowMapPass(PointLight* light)
{
	omniShadowShader.UseShader();

	glViewport(0, 0, light->getShadowMap()->GetShadowWidth(), light->getShadowMap()->GetShadowHeight());

	light->getShadowMap()->Write();
	glClear(GL_DEPTH_BUFFER_BIT);

	uniformModel = omniShadowShader.GetModelLocation();
	uniformOmniLightPos = omniShadowShader.GetOmniLightPosLocation();
	uniformFarPlane = omniShadowShader.GetFarPlaneLocation();

	glUniform3f(uniformOmniLightPos, light->GetPosition().x, light->GetPosition().y, light->GetPosition().z);
	glUniform1f(uniformFarPlane, light->GetFarPlane());
	omniShadowShader.SetLightMatrices(light->CalculateLightTransform());

	omniShadowShader.Validate();

	RenderScene();

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderPass(glm::mat4 viewMatrix, glm::mat4 projectionMatrix)
{
	glViewport(0, 0, 1366, 768);

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	skybox.DrawSkybox(viewMatrix, projectionMatrix);

	shaderList[0].UseShader();

	uniformModel = shaderList[0].GetModelLocation();
	uniformProjection = shaderList[0].GetProjectionLocation();
	uniformView = shaderList[0].GetViewLocation();
	uniformModel = shaderList[0].GetModelLocation();
	uniformEyePosition = shaderList[0].GetEyePositionLocation();
	uniformSpecularIntensity = shaderList[0].GetSpecularIntensityLocation();
	uniformShininess = shaderList[0].GetShininessLocation();

	glUniformMatrix4fv(uniformProjection, 1, GL_FALSE, glm::value_ptr(projectionMatrix));
	glUniformMatrix4fv(uniformView, 1, GL_FALSE, glm::value_ptr(viewMatrix));
	glUniform3f(uniformEyePosition, camera.getCameraPosition().x, camera.getCameraPosition().y, camera.getCameraPosition().z);

	shaderList[0].SetDirectionalLight(&mainLight);
	shaderList[0].SetPointLights(pointLights, pointLightCount, 3, 0);
	shaderList[0].SetSpotLights(spotLights, spotLightCount, 3 + pointLightCount, pointLightCount);
	//shaderList[0].SetDirectionalLightTransform(&mainLight.CalculateLightTransform());

	mainLight.getShadowMap()->Read(GL_TEXTURE2);
	shaderList[0].SetTexture(1);
	shaderList[0].SetDirectionalShadowMap(2);

	glm::vec3 lowerLight = camera.getCameraPosition();
	lowerLight.y -= 0.3f;
	spotLights[0].SetFlash(lowerLight, camera.getCameraDirection());

	shaderList[0].Validate();

	RenderScene();
}

int main() 
{
	mainWindow = Window(1366, 768); // 1280, 1024 or 1024, 768
	mainWindow.Initialise();

	CreateShaders();

	camera = Camera(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), -60.0f, 0.0f, 5.0f, 0.5f);

	plainTexture = Texture("Textures/plain.png");
	plainTexture.LoadTextureA();

	mainLight = DirectionalLight(2048, 2048,
								1.0f, 0.53f, 0.3f, 
								0.1f, 0.9f,
								-10.0f, -12.0f, 18.5f);

	pointLights[0] = PointLight(1024, 1024,
								0.01f, 100.0f,
								0.0f, 0.0f, 1.0f,
								0.0f, 1.0f,
								1.0f, 2.0f, 0.0f,
								0.3f, 0.2f, 0.1f);
	pointLightCount++;
	pointLights[1] = PointLight(1024, 1024,
								0.01f, 100.0f, 
								0.0f, 1.0f, 0.0f,
								0.0f, 1.0f,
								-4.0f, 3.0f, 0.0f,
								0.3f, 0.2f, 0.1f);
	pointLightCount++;

	
	spotLights[0] = SpotLight(1024, 1024,
						0.01f, 100.0f, 
						1.0f, 1.0f, 1.0f,
						0.0f, 2.0f,
						0.0f, 0.0f, 0.0f,
						0.0f, -1.0f, 0.0f,
						1.0f, 0.0f, 0.0f,
						20.0f);
	spotLightCount++;
	spotLights[1] = SpotLight(1024, 1024,
						0.01f, 100.0f, 
						1.0f, 1.0f, 1.0f,
						0.0f, 1.0f,
						0.0f, -1.5f, 0.0f,
						-100.0f, -1.0f, 0.0f,
						1.0f, 0.0f, 0.0f,
						20.0f);
	spotLightCount++;

	std::vector<std::string> skyboxFaces;
	skyboxFaces.push_back("Textures/Skybox/alpha-island/alpha-island_rt.tga");
	skyboxFaces.push_back("Textures/Skybox/alpha-island/alpha-island_lf.tga");
	skyboxFaces.push_back("Textures/Skybox/alpha-island/alpha-island_up.tga");
	skyboxFaces.push_back("Textures/Skybox/alpha-island/alpha-island_dn.tga");
	skyboxFaces.push_back("Textures/Skybox/alpha-island/alpha-island_bk.tga");
	skyboxFaces.push_back("Textures/Skybox/alpha-island/alpha-island_ft.tga");

	skybox = Skybox(skyboxFaces);

	GLuint uniformProjection = 0, uniformModel = 0, uniformView = 0, uniformEyePosition = 0,
		uniformSpecularIntensity = 0, uniformShininess = 0;

	glm::mat4 projection = glm::perspective(glm::radians(60.0f), (GLfloat)mainWindow.getBufferWidth() / mainWindow.getBufferHeight(), 0.1f, 100.0f);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	// Setup Platform/Renderer bindings
	ImGui_ImplGlfw_InitForOpenGL(mainWindow.getGLWFWindow(), true);
	ImGui_ImplOpenGL3_Init("#version 330");
	// Setup Dear ImGui style
	ImGui::StyleColorsDark();

	static bool createObject = false;
	static bool importObject = false;

	static bool createCube = false;
	static bool createSphere = false;

	static bool directionalLight = false;
	static int spotLight = -1;
	static bool openSpotLight = false;
	static int pointLight = -1;
	static bool openPointLight = false;

	static float FOV = 60.0f;
	static float near = 0.1f;
	static float far = 100.0f;

	static int currSkybox = 0;



	// Loop until window closed
	while (!mainWindow.getShouldClose())
	{
		GLfloat now = glfwGetTime(); // SDL_GetPerformanceCounter();
		deltaTime = now - lastTime; // (now - lastTime)*1000/SDL_GetPerformanceFrequency();
		lastTime = now;

		// Get + Handle User Input
		glfwPollEvents();

		camera.keyControl(mainWindow.getsKeys(), deltaTime);
		if (mainWindow.getsKeys()[GLFW_MOUSE_BUTTON_2]) {
			camera.mouseControl(mainWindow.getXChange(), mainWindow.getYChange());
		}

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		if (mainWindow.getsKeys()[GLFW_KEY_L])
		{
			spotLights[0].Toggle();
			mainWindow.getsKeys()[GLFW_KEY_L] = false;
		}

		DirectionalShadowMapPass(&mainLight);
		for (size_t i = 0; i < pointLightCount; i++)
		{
			OmniShadowMapPass(&pointLights[i]);
		}
		for (size_t i = 0; i < spotLightCount; i++)
		{
			OmniShadowMapPass(&spotLights[i]);
		}

		RenderPass(camera.calculateViewMatrix(), projection);

		// Object hierarchy
		ImGuiWindowFlags window_flags = 0;
		window_flags |= ImGuiWindowFlags_MenuBar;

		ImGui::Begin("Settings", NULL, window_flags);

			float moveSpeed = camera.getMoveSpeed();
			ImGui::DragFloat("Move speed", &moveSpeed, 0.01f, 0.0f, 10.0f);
			camera.setMoveSpeed(moveSpeed);

			float turnSpeed = camera.getTurnSpeed();
			ImGui::DragFloat("Turn speed", &turnSpeed, 0.01f, 0.0f, 10.0f);
			camera.setTurnSpeed(turnSpeed);

			ImGui::DragFloat("Field of view", &FOV, 0.1f, 30.0f, 180.0f);
			ImGui::DragFloat("Near plane", &near, 0.1f, 0.0f, far);
			ImGui::DragFloat("Far plane", &far, 0.1f, near);
			projection = glm::perspective(glm::radians(FOV), (GLfloat)mainWindow.getBufferWidth() / mainWindow.getBufferHeight(), near, far);

			int prevSkybox = currSkybox;
			const char* skyboxes[] = { "alpha-island", "alps", "arctic", "arrakisday", "ashcanyon", "badomen", "blue", "boulder-bay", 
									   "bromene-bay", "cloudtop", "cottoncandy", "coward-cove", "craterflake", "criminal-element", 
									   "cupertin-lake", "darkgloom", "day-at-the-beach", "desertsky", "emeraldfog", "fadeaway", 
									   "fat-chance-in-hell", "frozen", "frozendusk", "glacier", "greenhaze", "greywash", "habitual-pain", 
									   "hills", "hills2", "iceflats", "iceflow", "interstellar", "lagoon", "leatherduster", "lilacisles", 
									   "limon", "lmcity", "majesty", "midnight-silence", "miramar", "mountain", "mudskipper", "nevada", 
									   "nightsky", "oasisnight", "peaks", "plains-of-abraham", "powderpeak", "purplenebula", "redplanet", 
									   "rockyvalley", "seeingred", "sep", "shadowpeak", "siege", "snow", "snowy", "sorbin", "ss", "starfield", 
									   "stormydays", "stratosphere", "sunset", "violentdays" };
			ImGui::Combo("Skybox", &currSkybox, skyboxes, IM_ARRAYSIZE(skyboxes));

			if (prevSkybox != currSkybox) {
				std::vector<std::string> skyboxFaces;
				std::string skyboxName = skyboxes[currSkybox];
				skyboxFaces.push_back("Textures/Skybox/" + skyboxName + "/" + skyboxName + "_rt.tga");
				skyboxFaces.push_back("Textures/Skybox/" + skyboxName + "/" + skyboxName + "_lf.tga");
				skyboxFaces.push_back("Textures/Skybox/" + skyboxName + "/" + skyboxName + "_up.tga");
				skyboxFaces.push_back("Textures/Skybox/" + skyboxName + "/" + skyboxName + "_dn.tga");
				skyboxFaces.push_back("Textures/Skybox/" + skyboxName + "/" + skyboxName + "_bk.tga");
				skyboxFaces.push_back("Textures/Skybox/" + skyboxName + "/" + skyboxName + "_ft.tga");

				skybox = Skybox(skyboxFaces);
				prevSkybox = currSkybox;
			}
			

		ImGui::End();

		ImGui::Begin("Object hierarchy", NULL, window_flags);

			ImGui::PushItemWidth(ImGui::GetFontSize() * -12);

			if (ImGui::BeginMenuBar())
			{
				if (ImGui::BeginMenu("File"))
				{
					/*if (ImGui::BeginMenu("New")) {
						if (ImGui::BeginMenu("Object")) {
							ImGui::MenuItem("Cube", NULL, &createCube);
							ImGui::MenuItem("Sphere", NULL, &createSphere);
							ImGui::EndMenu();
						}
						ImGui::EndMenu();
					}*/
					ImGui::MenuItem("Import", NULL, &importObject);
					ImGui::EndMenu();
				}

				ImGui::EndMenuBar();
			}

			if (ImGui::CollapsingHeader("Objects")) {
				if (objects.size() == 0) {
					ImGui::Text("No objects on display!");
				}
				else {
					int ID = 0;
					for (Object* object : objects) {
						ImGui::PushID(ID);
						if (ImGui::Button("X")) {
							objects.erase(std::find(objects.begin(), objects.end(), object));
						}
						ImGui::PopID();
						ImGui::SameLine();
						if (ImGui::Selectable(object->getName(), false, ImGuiSelectableFlags_AllowDoubleClick))
							if (ImGui::IsMouseDoubleClicked(0))
								object->setIsSelected(true);
						ID++;
					}
				}
			}

			if (ImGui::CollapsingHeader("Lights")) {
				if (ImGui::TreeNode("Spotlights")) {
					int ID = 0;
					for (SpotLight light : spotLights) {
						ID++;
						std::string lightNameStr = "Spotlight " + std::to_string(ID);
						const char* lightName = lightNameStr.c_str();
						if (ImGui::Selectable(lightName, false, ImGuiSelectableFlags_AllowDoubleClick)) {
							if (ImGui::IsMouseDoubleClicked(0)) {
								spotLight = ID;
								openSpotLight = true;
							}
						}
					}
					ImGui::TreePop();
				}

				if (ImGui::TreeNode("Pointlights")) {
					int ID = 0;
					for (PointLight light : pointLights) {
						ID++;
						std::string lightNameStr = "Pointlight " + std::to_string(ID);
						const char* lightName = lightNameStr.c_str();
						if (ImGui::Selectable(lightName, false, ImGuiSelectableFlags_AllowDoubleClick)) {
							if (ImGui::IsMouseDoubleClicked(0)) {
								pointLight = ID;
								openPointLight = true;
							}
						}
					}
					ImGui::TreePop();
				}
			}
		ImGui::End();

		if (spotLight > 0) {
			std::string lightNameStr = "Spotlight " + std::to_string(spotLight);
			const char* lightName = lightNameStr.c_str();
			if(ImGui::Begin(lightName, &openSpotLight)) {

				if (ImGui::Button("Toggle light")) {
					spotLights[spotLight - 1].Toggle();
				}

				float ambientIntensity = spotLights[spotLight - 1].getAmbientIntensity();
				ImGui::DragFloat("Ambient intensity", &ambientIntensity, 0.1f, 0.0f, 300.0f);
				spotLights[spotLight - 1].setAmbientIntensity(ambientIntensity);

				float diffuseIntensity = spotLights[spotLight - 1].getDiffuseIntensity();
				ImGui::DragFloat("Diffuse intensity", &diffuseIntensity, 0.1f, 0.0f, 300.0f);
				spotLights[spotLight - 1].setDiffuseIntensity(diffuseIntensity);

				float colours[3] = { spotLights[spotLight-1].getColour().x,
									spotLights[spotLight-1].getColour().y,
									spotLights[spotLight-1].getColour().z };
				ImGui::ColorPicker3("Light colour", colours);
				spotLights[spotLight-1].setColour(colours);

				if (!openSpotLight) {
					spotLight = -1;
				}
				ImGui::End();
			}
		}

		if (pointLight > 0) {
			std::string lightNameStr = "Pointlight " + std::to_string(pointLight);
			const char* lightName = lightNameStr.c_str();
			if (ImGui::Begin(lightName, &openPointLight)) {

				float ambientIntensity = pointLights[pointLight - 1].getAmbientIntensity();
				ImGui::DragFloat("Ambient intensity", &ambientIntensity, 0.1f, 0.0f, 300.0f);
				pointLights[pointLight - 1].setAmbientIntensity(ambientIntensity);

				float diffuseIntensity = pointLights[pointLight - 1].getDiffuseIntensity();
				ImGui::DragFloat("Diffuse intensity", &diffuseIntensity, 0.1f, 0.0f, 300.0f);
				pointLights[pointLight - 1].setDiffuseIntensity(diffuseIntensity);

				float colours[3] = { pointLights[pointLight-1].getColour().x,
					pointLights[pointLight-1].getColour().y,
					pointLights[pointLight-1].getColour().z };
				ImGui::ColorPicker3("Light colour", colours);
				printf("Colour: %f\n", colours[1]);
				pointLights[pointLight-1].setColour(colours);

				if (!openPointLight) {
					pointLight = -1;
				}
				ImGui::End();
			}
		}

		if (importObject) {
			ImGui::Begin("Import object", &importObject);
				std::string path = "Models";
				for (const auto& entry : std::filesystem::directory_iterator(path)) {
					std::string fileName = entry.path().string();
					if (fileName.find(".obj") != std::string::npos) {
						fileName = fileName.substr(7);
						if (ImGui::Selectable(fileName.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick)) {
							if (ImGui::IsMouseDoubleClicked(0)) {
								Model* model = new Model();
								model->LoadModel(entry.path().string());

								std::string* name = new std::string(fileName.substr(0, fileName.find(".obj")));
								Object* object = new Object(*model, *name);

								objects.push_back(object);

								importObject = false;
							}
						}
					}
				}
			ImGui::End();
		}

		for (Object* object : objects) {
			if (object->getIsSelected()) {
				bool isSelected = object->getIsSelected();
				ImGui::Begin(object->getName(), &isSelected);

					

					float position[4] = { object->getPos().x, object->getPos().y, object->getPos().z, 0.0f };
					float rotation[4] = { object->getRot().x, object->getRot().y, object->getRot().z, 0.0f };
					float scale[4] = { object->getScale().x, object->getScale().y, object->getScale().z, 0.0f };

					ImGui::DragFloat3("Position", position, 0.1f);
					ImGui::DragFloat3("Rotation", rotation, 0.1f);
					ImGui::DragFloat3("Scale", scale, 0.1f);

					object->setPos(position[0], position[1], position[2]);
					object->setRot(rotation[0], rotation[1], rotation[2]);
					object->setScale(scale[0], scale[1], scale[2]);

				ImGui::End();

				object->setIsSelected(isSelected);
			}
		}

		ImGui::ShowDemoWindow();

		// Render dear imgui into screen
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		mainWindow.swapBuffers();
	}

	return 0;
}