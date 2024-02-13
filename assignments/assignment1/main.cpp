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
void drawUI();

//Global state
int screenWidth = 1080;
int screenHeight = 720;
float prevFrameTime;
float deltaTime;
ew::Transform monkeyTransform;
ew::Camera camera;
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
	float Brightness = 1.0;
	glm::vec3 colorFilter = glm::vec3(1);
}colorCorrect;


int main() {
	GLFWwindow* window = initWindow("Assignment 0", screenWidth, screenHeight);
	
	// Shader and Model Setup
	ew::Shader sceneShader = ew::Shader("assets/lit.vert", "assets/lit.frag");
	ew::Shader postProcessShader = ew::Shader("assets/postprocess.vert", "assets/postprocess.frag");
	ew::Model monkeyModel = ew::Model("assets/suzanne.obj");
	ew::Mesh planeMesh = ew::Mesh(ew::createPlane(10, 10, 5));

	// Texture Loading
	GLuint brickTexture = ew::loadTexture("assets/brick_color.jpg");

	// Camera Setup
	camera.position = glm::vec3(0.0f, 0.0f, 5.0f);
	camera.target = glm::vec3(0.0f, 0.0f, 0.0f);
	camera.aspectRatio = (float)screenWidth / screenHeight;
	camera.fov = 60.0f;

	// FBO Creation
	unsigned int fbo;
	glCreateFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);

	// Color Buffer Creation
	unsigned int frameBufferTexture;
	glGenTextures(1, &frameBufferTexture);
	glBindTexture(GL_TEXTURE_2D, frameBufferTexture);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA16, screenWidth, screenHeight);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// Depth Buffer Creation
	unsigned int depth_texture;
	glGenTextures(1, &depth_texture);
	glBindTexture(GL_TEXTURE_2D, depth_texture);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH_COMPONENT16, screenWidth, screenHeight);

	// Assigning
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, frameBufferTexture, 0);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depth_texture, 0);

	// Dummy VAO
	unsigned int dummyVAO;
	glCreateVertexArrays(1, &dummyVAO);

	// Draw Buffer Setup (Just in case of multiple render textures)
	static const GLenum draw_buffers[] = { GL_COLOR_ATTACHMENT0 };
	glDrawBuffers(1, draw_buffers);

	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glEnable(GL_DEPTH_TEST);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);

	// Render Loop
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		float time = (float)glfwGetTime();
		deltaTime = time - prevFrameTime;
		prevFrameTime = time;

		monkeyTransform.rotation = glm::rotate(monkeyTransform.rotation, deltaTime, glm::vec3(0.0, 1.0, 0.0));
		cameraController.move(window, &camera, deltaTime);

		// FIRST PASS (Custom Framebuffer Pass)
		glBindFramebuffer(GL_FRAMEBUFFER, fbo);
		glViewport(0, 0, screenWidth, screenHeight);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Draw Scene General Scene
		glBindTextureUnit(0, brickTexture);
		sceneShader.use();
		sceneShader.setInt("_MainTex", 0);
		sceneShader.setFloat("_Material.AmbientCo", material.AmbientCo);
		sceneShader.setFloat("_Material.DiffuseCo", material.DiffuseCo);
		sceneShader.setFloat("_Material.SpecualarCo", material.SpecualarCo);
		sceneShader.setFloat("_Material.Shininess", material.Shininess);
		sceneShader.setVec3("_EyePos", camera.position);
		sceneShader.setMat4("_Model", monkeyTransform.modelMatrix());
		sceneShader.setMat4("_ViewProjection", camera.projectionMatrix() * camera.viewMatrix());
		monkeyModel.draw();
		planeMesh.draw();

		// SECOND PASS (Back to Base Backbuffer)
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Apply Color Correction + Tonemapping
		postProcessShader.use();
		postProcessShader.setFloat("_Exposure", colorCorrect.Exposure);
		postProcessShader.setFloat("_Contrast", colorCorrect.Contrast);
		postProcessShader.setFloat("_Brightness", colorCorrect.Brightness);
		postProcessShader.setVec3("_ColorFiltering", colorCorrect.colorFilter);

		// Fullscreen Quad
		glBindTextureUnit(0, frameBufferTexture);
		glBindVertexArray(dummyVAO);
		glDrawArrays(GL_TRIANGLES, 0, 6);

		drawUI();

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	glDeleteFramebuffers(1, &fbo);

	printf("Shutting down...");
}

void resetCamera(ew::Camera* camera, ew::CameraController* controller) {
	camera->position = glm::vec3(0, 0, 5.0f);
	camera->target = glm::vec3(0);
	controller->yaw = controller->pitch = 0;
}

void drawUI() {
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

	// Color Correction ImGUI
	if (ImGui::CollapsingHeader("Color Correction"))
	{
		ImGui::SliderFloat("Exposure", &colorCorrect.Exposure, 0.0f, 2.0f);
		ImGui::SliderFloat("Contrast", &colorCorrect.Contrast, 0.0f, 2.0f);
		ImGui::SliderFloat("Brightness", &colorCorrect.Brightness, 0.0f, 2.0f);
		ImGui::ColorEdit3("Color Filtering", &colorCorrect.colorFilter.r);
	}

	// Camera Control ImGUI
	if (ImGui::Button("Reset Camera")) 
	{
		resetCamera(&camera, &cameraController);
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

