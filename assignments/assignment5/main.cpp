#include <stdio.h>
#include <math.h>

#include <ew/external/glad.h>

#include <ew/shader.h>
#include <ew/model.h>
#include <ew/camera.h>
#include <ew/transform.h>
#include <ew/cameraController.h>
#include <ew/texture.h>
#include <ew/procGen.h>

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

void framebufferSizeCallback(GLFWwindow* window, int width, int height);
GLFWwindow* initWindow(const char* title, int width, int height);


//Global state
int screenWidth = 1080;
int screenHeight = 720;
float prevFrameTime;
float deltaTime;
ew::Transform monkeyTransform;
ew::Transform planeTransform;
ew::Camera camera;
ew::Camera lightCamera;
ew::CameraController cameraController;

struct Material {
	float AmbientCo = 1.0;
	float DiffuseCo = 0.5;
	float SpecualarCo = 0.5;
	float Shininess = 128;
}material;

struct ColorCorrect {
	float Exposure = 1.0;
	float Contrast = 1.0;
	float Brightness = 0.0;
	glm::vec3 colorFilter = glm::vec3(1);
}colorCorrect;

struct Light {
	glm::vec3 lightDirection = glm::vec3(0.0, -1.0, 0.0);
	glm::vec3 lightColor = glm::vec3(1);
}light;

struct PointLight {
	glm::vec3 position;
	float radius;
	glm::vec4 color;
};
const int MAX_POINT_LIGHTS = 64;
PointLight pointLights[MAX_POINT_LIGHTS];

struct Shadow {
	float minBias = 0.007;
	float maxBias = 0.2;
}shadow;

struct Framebuffer {
	unsigned int fbo;
	unsigned int colorTexture[8];
	unsigned int depthTexture;
	unsigned int width;
	unsigned int height;
}framebuffer;

struct Node {
	glm::mat4 localTransform;
	glm::mat4 globalTransform;
	unsigned int parentIndex;
	ew::Transform transformData;
};

struct Hierarchy {
	std::vector<Node*> nodes;
};

void drawUI(Framebuffer& gBuffer, unsigned int shadowMap);

Framebuffer createFrameBuffer(unsigned int width, unsigned int height, int colorFormat)
{
	Framebuffer buffer;

	buffer.width = width;
	buffer.height = height;

	glCreateFramebuffers(1, &buffer.fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, buffer.fbo);

	// Color Buffer Creation
	glGenTextures(1, &buffer.colorTexture[0]);
	glBindTexture(GL_TEXTURE_2D, buffer.colorTexture[0]);
	glTexStorage2D(GL_TEXTURE_2D, 1, colorFormat, buffer.width, buffer.height);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// Depth Buffer Creation
	glGenTextures(1, &buffer.depthTexture);
	glBindTexture(GL_TEXTURE_2D, buffer.depthTexture);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH_COMPONENT16, screenWidth, screenHeight);

	// Assigning
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, buffer.colorTexture[0], 0);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, buffer.depthTexture, 0);

	return buffer;
}

Framebuffer createGBuffer(unsigned int width, unsigned int height)
{
	Framebuffer buffer;
	buffer.width = width;
	buffer.height = height;

	glCreateFramebuffers(1, &buffer.fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, buffer.fbo);

	glEnable(GL_DEPTH_TEST);

	int inputs[3]
	{
		GL_RGB32F, // World Pos
		GL_RGB16F, // World Normal
		GL_RGB16F // Albedo Color
	};

	for (size_t i = 0; i < 3; i++)
	{
		glGenTextures(1, &buffer.colorTexture[i]);
		glBindTexture(GL_TEXTURE_2D, buffer.colorTexture[i]);
		glTexStorage2D(GL_TEXTURE_2D, 1, inputs[i], width, height);
		//Clamp to border so we don't wrap when sampling for post processing
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		//Attach each texture to a different slot.
	//GL_COLOR_ATTACHMENT0 + 1 = GL_COLOR_ATTACHMENT1, etc
		glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, buffer.colorTexture[i], 0);
	}

	const GLenum drawBuffers[3] = {
		GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2
	};

	glDrawBuffers(3, drawBuffers);


	glGenTextures(1, &buffer.depthTexture);
	glBindTexture(GL_TEXTURE_2D, buffer.depthTexture);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH_COMPONENT16, screenWidth, screenHeight);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, buffer.depthTexture, 0);

	// Add depth buffer?
	// Check for bugs

	glBindTexture(GL_TEXTURE_2D, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	return buffer;
}

void GetFK(Hierarchy h)
{
	for each (Node* node in h.nodes)
	{
		if (node->parentIndex == -1)
			node->globalTransform = node->localTransform;
		else
			node->globalTransform = h.nodes[node->parentIndex]->globalTransform * node->localTransform;
	}
}

int main() {
	GLFWwindow* window = initWindow("Assignment 3", screenWidth, screenHeight);

	// Shader and Model Setup
	ew::Shader sceneShader = ew::Shader("assets/lit.vert", "assets/lit.frag");
	ew::Shader deferredShader = ew::Shader("assets/postprocess.vert", "assets/deferredLit.frag");
	ew::Shader geometryShader = ew::Shader("assets/lit.vert", "assets/geometryPass.frag");
	ew::Shader postProcessShader = ew::Shader("assets/postprocess.vert", "assets/postprocess.frag");
	ew::Shader shadowShader = ew::Shader("assets/depthOnly.vert", "assets/depthOnly.frag");
	ew::Shader lightOrbShader = ew::Shader("assets/lightOrb.vert", "assets/lightOrb.frag");
	ew::Model monkeyModel = ew::Model("assets/suzanne.obj");
	ew::Mesh planeMesh = ew::Mesh(ew::createPlane(10, 10, 5));
	ew::Mesh sphereMesh = ew::Mesh(ew::createSphere(1.0f, 8));

	// Texture Loading
	GLuint floorTexture = ew::loadTexture("assets/Floor_Color.jpg");
	GLuint monkeyTexture = ew::loadTexture("assets/Monkey_Color.jpg");

	// Camera Setup
	camera.position = glm::vec3(0.0f, 0.0f, 5.0f);
	camera.target = glm::vec3(0.0f, 0.0f, 0.0f);
	camera.aspectRatio = (float)screenWidth / screenHeight;
	camera.fov = 60.0f;

	planeTransform.position = glm::vec3(0.0f, -1.0f, 0.0f);

	lightCamera.target = glm::vec3(17.5f, 0.0f, 17.5f);
	lightCamera.position = lightCamera.target - light.lightDirection * 10.0f;
	lightCamera.orthographic = true;
	lightCamera.orthoHeight = 40.0f;
	lightCamera.nearPlane = 0.001f;
	lightCamera.farPlane = 50.0f;
	lightCamera.aspectRatio = 1;

	// FK Implementation
	Hierarchy bones;

	Node torso;
	Node arm;
	Node hand;

	torso.transformData.position = glm::vec3(0, 0, 0);
	arm.transformData.position = glm::vec3(1, 0, 0);
	arm.transformData.scale = glm::vec3(0.3, 0.3, 0.3);
	hand.transformData.position = glm::vec3(2, -1.5, 0);
	hand.transformData.scale = glm::vec3(0.5, 0.5, 0.5);

	bones.nodes.push_back(&torso);
	bones.nodes.push_back(&arm);
	bones.nodes.push_back(&hand);

	torso.parentIndex = -1;
	arm.parentIndex = 0;
	hand.parentIndex = 1;


	Framebuffer ppFBO = createFrameBuffer(screenWidth, screenHeight, GL_RGBA16);
	Framebuffer lightOrbs = createFrameBuffer(screenWidth, screenHeight, GL_RGBA16);
	Framebuffer GBuffer = createGBuffer(screenWidth, screenHeight);

	// Shadow Map and Buffer Creation
	unsigned int shadowFBO, shadowMap;
	glCreateFramebuffers(1, &shadowFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);

	glGenTextures(1, &shadowMap);
	glBindTexture(GL_TEXTURE_2D, shadowMap);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH_COMPONENT16, 2048, 2048);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	//Pixels outside of frustum should have max distance (white)
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	float borderColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, shadowMap, 0);

	// Dummy VAO
	unsigned int dummyVAO;
	glCreateVertexArrays(1, &dummyVAO);

	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	glEnable(GL_CULL_FACE);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);

	deferredShader.use();
	int index = 0;
	for (int x = -1; x < 1; x++)
	{
		for (int y = -1; y <= 1; y++)
		{
			pointLights[index].position = glm::vec3((x * 4) + 1, -0.5, (y * 4) + 1);
			pointLights[index].radius = 5.0;
			pointLights[index].color = glm::vec4(rand() % 2, rand() % 2, rand() % 2, 1);
			index++;
		}
	}


	glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);

	// Render Loop
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		float time = (float)glfwGetTime();
		deltaTime = time - prevFrameTime;
		prevFrameTime = time;

		
		cameraController.move(window, &camera, deltaTime);

		lightCamera.position = lightCamera.target - light.lightDirection * 10.0f;

		glm::mat4 lightView = lightCamera.viewMatrix();
		glm::mat4 lightProj = lightCamera.projectionMatrix();
		glm::mat4 lightMatrix = lightProj * lightView;

		
		torso.transformData.rotation = glm::rotate(torso.transformData.rotation, deltaTime, glm::vec3(0.0, 1.0, 0.0));
		arm.transformData.rotation = glm::rotate(arm.transformData.rotation, deltaTime, glm::vec3(1.0, 0.0, 0.0));
		hand.transformData.rotation = glm::rotate(hand.transformData.rotation, deltaTime, glm::vec3(1.0, 0.0, 0.0));
		

		for each (Node *node in bones.nodes)
		{
			node->localTransform = node->transformData.modelMatrix();
		}
		
		GetFK(bones);



		// FIRST PASS SHADOW BUFFER
		glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);
		glViewport(0, 0, 2048, 2048);
		glClear(GL_DEPTH_BUFFER_BIT);
		glCullFace(GL_FRONT);
		//glDepthFunc(GL_LESS);

		shadowShader.use();
		planeTransform.position = glm::vec3(0, -1, 0);
		shadowShader.setMat4("_ViewProjection", lightMatrix);
		shadowShader.setMat4("_Model", torso.globalTransform);
		monkeyModel.draw();
		shadowShader.setMat4("_Model", arm.globalTransform);
		monkeyModel.draw();
		shadowShader.setMat4("_Model", hand.globalTransform);
		monkeyModel.draw();
		shadowShader.setMat4("_Model", planeTransform.modelMatrix());
		planeMesh.draw();
		//glDepthFunc(GL_EQUAL);
		glBindFramebuffer(GL_FRAMEBUFFER, GBuffer.fbo);
		glViewport(0, 0, GBuffer.width, GBuffer.height);
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glCullFace(GL_BACK);

		glBindTextureUnit(0, shadowMap);
		glBindTextureUnit(1, monkeyTexture);
		glBindTextureUnit(2, floorTexture);

		geometryShader.use();
		//geometryShader.setMat4("_LightViewProjection", lightMatrix);
		geometryShader.setMat4("_ViewProjection", camera.projectionMatrix() * camera.viewMatrix());

		geometryShader.setInt("_MainTex", 1);
		geometryShader.setMat4("_Model", torso.globalTransform);
		monkeyModel.draw();
		geometryShader.setMat4("_Model", arm.globalTransform);
		monkeyModel.draw();
		geometryShader.setMat4("_Model", hand.globalTransform);
		monkeyModel.draw();
		geometryShader.setInt("_MainTex", 2);
		geometryShader.setMat4("_Model", planeTransform.modelMatrix());
		planeMesh.draw();

		// SECOND PASS (Custom Framebuffer Pass)
		glBindFramebuffer(GL_FRAMEBUFFER, ppFBO.fbo);
		glViewport(0, 0, screenWidth, screenHeight);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Draw Scene General Scene
		deferredShader.use();

		glBindTextureUnit(0, GBuffer.colorTexture[0]);
		glBindTextureUnit(1, GBuffer.colorTexture[1]);
		glBindTextureUnit(2, GBuffer.colorTexture[2]);
		glBindTextureUnit(3, shadowMap);

		deferredShader.setInt("_ShadowMap", 3);
		deferredShader.setMat4("_LightViewProjection", lightMatrix);
		deferredShader.setVec3("_LightDirection", light.lightDirection);
		deferredShader.setVec3("_LightColor", light.lightColor);
		deferredShader.setFloat("_MinBias", shadow.minBias);
		deferredShader.setFloat("_MaxBias", shadow.maxBias);
		deferredShader.setFloat("_Material.AmbientCo", material.AmbientCo);
		deferredShader.setFloat("_Material.DiffuseCo", material.DiffuseCo);
		deferredShader.setFloat("_Material.SpecualarCo", material.SpecualarCo);
		deferredShader.setFloat("_Material.Shininess", material.Shininess);
		deferredShader.setVec3("_EyePos", camera.position);


		for (int i = 0; i < MAX_POINT_LIGHTS; i++) {
			//Creates prefix "_PointLights[0]." etc
			std::string prefix = "_PointLights[" + std::to_string(i) + "].";
			deferredShader.setVec3(prefix + "position", pointLights[i].position);
			deferredShader.setFloat(prefix + "radius", pointLights[i].radius);
			deferredShader.setVec4(prefix + "color", pointLights[i].color);
		}

		glBindVertexArray(dummyVAO);
		glDrawArrays(GL_TRIANGLES, 0, 6);

		glBindFramebuffer(GL_READ_FRAMEBUFFER, GBuffer.fbo); //Read from gBuffer 
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, ppFBO.fbo); //Write to current fbo
		glBlitFramebuffer(0, 0, screenWidth, screenHeight, 0, 0, screenWidth, screenHeight, GL_DEPTH_BUFFER_BIT, GL_NEAREST);

		//Draw all light orbs
		lightOrbShader.use();
		lightOrbShader.setMat4("_ViewProjection", camera.projectionMatrix() * camera.viewMatrix());
		for (int i = 0; i < MAX_POINT_LIGHTS; i++)
		{
			glm::mat4 m = glm::mat4(1.0f);
			m = glm::translate(m, pointLights[i].position);
			m = glm::scale(m, glm::vec3(0.2f)); 

			lightOrbShader.setMat4("_Model", m);
			lightOrbShader.setVec3("_Color", pointLights[i].color);
			sphereMesh.draw();
		}



		// SECOND PASS (Back to Base Backbuffer)
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Apply Color Correction + Tonemapping
		postProcessShader.use();
		postProcessShader.setFloat("_Exposure", colorCorrect.Exposure);
		postProcessShader.setFloat("_Contrast", colorCorrect.Contrast);
		postProcessShader.setFloat("_Brightness", colorCorrect.Brightness);

		// Fullscreen Quad
		glBindTextureUnit(0, ppFBO.colorTexture[0]);
		glBindVertexArray(dummyVAO);
		glDrawArrays(GL_TRIANGLES, 0, 6);


		drawUI(GBuffer, shadowMap);

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	glDeleteFramebuffers(1, &ppFBO.fbo);

	printf("Shutting down...");
}

void resetCamera(ew::Camera* camera, ew::CameraController* controller) {
	camera->position = glm::vec3(0, 0, 5.0f);
	camera->target = glm::vec3(0);
	controller->yaw = controller->pitch = 0;
}

void drawUI(Framebuffer& gBuffer, unsigned int shadowMap) {
	ImGui_ImplGlfw_NewFrame();
	ImGui_ImplOpenGL3_NewFrame();
	ImGui::NewFrame();

	ImGui::Begin("Settings");


	// Material ImGUI (Lighting)
	if (ImGui::CollapsingHeader("Material")) 
	{
		ImGui::SliderFloat("AmbientK", &material.AmbientCo, 0.0f, 1.0f);
		ImGui::SliderFloat("DiffuseK", &material.DiffuseCo, 0.0f, 1.0f);
		ImGui::SliderFloat("SpecularK", &material.SpecualarCo, 0.0f, 1.0f);
		ImGui::SliderFloat("Shininess", &material.Shininess, 2.0f, 1024.0f);
	}

	if (ImGui::CollapsingHeader("Lighting"))
	{
		ImGui::SliderFloat("Light Direction X", &light.lightDirection.x, -1.0f, 1.0f);
		ImGui::SliderFloat("Light Direction Y", &light.lightDirection.y, -1.0f, 1.0f);
		ImGui::SliderFloat("Light Direction Z", &light.lightDirection.z, -1.0f, 1.0f);
		ImGui::ColorEdit3("Light Color", &light.lightColor.r);

		if (ImGui::CollapsingHeader("Shadow"))
		{
			ImGui::SliderFloat("Min Bias", &shadow.minBias, 0.0f, 1.0f);
			ImGui::SliderFloat("Max Bias", &shadow.maxBias, 0.0f, 1.0f);
		}
	}

	// Color Correction ImGUI
	if (ImGui::CollapsingHeader("Color Correction"))
	{
		ImGui::SliderFloat("Exposure", &colorCorrect.Exposure, 0.0f, 2.0f);
		ImGui::SliderFloat("Contrast", &colorCorrect.Contrast, 0.0f, 2.0f);
		ImGui::SliderFloat("Brightness", &colorCorrect.Brightness, 0.0f, 2.0f);
	}

	// Camera Control ImGUI
	if (ImGui::Button("Reset Camera")) 
	{
		resetCamera(&camera, &cameraController);
	}
	ImGui::End();

	ImGui::Begin("Shadow Map");
	ImGui::BeginChild("Shadow Map");
	//Stretch image to be window size
	ImVec2 windowSize = ImGui::GetWindowSize();
	//Invert 0-1 V to flip vertically for ImGui display
	//shadowMap is the texture2D handle
	ImGui::Image((ImTextureID)shadowMap, windowSize, ImVec2(0, 1), ImVec2(1, 0));
	ImGui::EndChild();

	ImGui::End();

	ImGui::Begin("GBuffers");
	ImVec2 texSize = ImVec2(gBuffer.width / 4, gBuffer.height / 4);
	for (size_t i = 0; i < 3; i++)
	{
		ImGui::Image((ImTextureID)gBuffer.colorTexture[i], texSize, ImVec2(0, 1), ImVec2(1, 0));
	}
	ImGui::End();

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void framebufferSizeCallback(GLFWwindow* window, int width, int height)
{
	glViewport(0, 0, width, height);
	screenWidth = width;
	screenHeight = height;
}

/// <summary>
/// Initializes GLFW, GLAD, and IMGUI
/// </summary>
/// <param name="title">Window title</param>
/// <param name="width">Window width</param>
/// <param name="height">Window height</param>
/// <returns>Returns window handle on success or null on fail</returns>
GLFWwindow* initWindow(const char* title, int width, int height) {
	printf("Initializing...");
	if (!glfwInit()) {
		printf("GLFW failed to init!");
		return nullptr;
	}

	GLFWwindow* window = glfwCreateWindow(width, height, title, NULL, NULL);
	if (window == NULL) {
		printf("GLFW failed to create window");
		return nullptr;
	}
	glfwMakeContextCurrent(window);

	if (!gladLoadGL(glfwGetProcAddress)) {
		printf("GLAD Failed to load GL headers");
		return nullptr;
	}

	//Initialize ImGUI
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init();

	return window;
}

