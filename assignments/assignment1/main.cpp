#include <stdio.h>
#include <math.h>

#include <ew/external/glad.h>

#include <ew/shader.h>
#include <ew/model.h>
#include <ew/camera.h>
#include <ew/transform.h>
#include <ew/cameraController.h>
#include <ew/texture.h>


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

	// Camera Setup
	camera.position = glm::vec3(0.0f, 0.0f, 5.0f);
	camera.target = glm::vec3(0.0f, 0.0f, 0.0f);
	camera.aspectRatio = (float)screenWidth / screenHeight;
	camera.fov = 60.0f;

	/*// Frame Buffer Stuff Assignment 1
	unsigned int fbo;
	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	
	unsigned int frameBufferTexture;
	glGenTextures(1, &frameBufferTexture);
	glBindTexture(GL_TEXTURE_2D, frameBufferTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, screenWidth, screenHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0);

	// Render Buffers

	glGenRenderbuffers(1, &rbo);
	glBindRenderbuffer(GL_RENDERBUFFER, rbo);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, screenWidth, screenHeight);
	glBindRenderbuffer(GL_RENDERBUFFER, 0);
	glFramebufferRenderbuffer(GL_RENDERBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);
	
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE)
	{
		printf("YIPEEEEEEEEEE");
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);*/

	unsigned int fbo;
	glCreateFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);

	unsigned int frameBufferTexture;
	glGenTextures(1, &frameBufferTexture);
	glBindTexture(GL_TEXTURE_2D, frameBufferTexture);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA16, screenWidth, screenHeight);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	unsigned int depth_texture;
	glGenTextures(1, &depth_texture);
	glBindTexture(GL_TEXTURE_2D, depth_texture);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH_COMPONENT16, screenWidth, screenHeight);


	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, frameBufferTexture, 0);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depth_texture, 0);

	unsigned int dummyVAO;
	glCreateVertexArrays(1, &dummyVAO);


	static const GLenum draw_buffers[] = { GL_COLOR_ATTACHMENT0 };
	glDrawBuffers(1, draw_buffers);

	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glEnable(GL_DEPTH_TEST);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		float time = (float)glfwGetTime();
		deltaTime = time - prevFrameTime;
		prevFrameTime = time;

		monkeyTransform.rotation = glm::rotate(monkeyTransform.rotation, deltaTime, glm::vec3(0.0, 1.0, 0.0));
		cameraController.move(window, &camera, deltaTime);

		// FIRST PASS
		glBindFramebuffer(GL_FRAMEBUFFER, fbo);
		glViewport(0, 0, screenWidth, screenHeight);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


		sceneShader.use();
		sceneShader.setFloat("_Material.AmbientCo", material.AmbientCo);
		sceneShader.setFloat("_Material.DiffuseCo", material.DiffuseCo);
		sceneShader.setFloat("_Material.SpecualarCo", material.SpecualarCo);
		sceneShader.setFloat("_Material.Shininess", material.Shininess);
		sceneShader.setVec3("_EyePos", camera.position);
		sceneShader.setMat4("_Model", monkeyTransform.modelMatrix());
		sceneShader.setMat4("_ViewProjection", camera.projectionMatrix() * camera.viewMatrix());
		monkeyModel.draw();

		// SECOND PASS
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		postProcessShader.use();
		postProcessShader.setFloat("_Exposure", colorCorrect.Exposure);
		postProcessShader.setFloat("_Contrast", colorCorrect.Contrast);
		postProcessShader.setFloat("_Brightness", colorCorrect.Brightness);
		postProcessShader.setVec3("_ColorFiltering", colorCorrect.colorFilter);

		
		glBindTextureUnit(0, frameBufferTexture);
		glBindVertexArray(dummyVAO);
		glDrawArrays(GL_TRIANGLES, 0, 6);

		drawUI();

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	//glDeleteFramebuffers(1, &fbo);
	//glDeleteRenderbuffers(1, &rbo);

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

	if (ImGui::CollapsingHeader("Material")) 
	{
		ImGui::SliderFloat("AmbientK", &material.AmbientCo, 0.0f, 1.0f);
		ImGui::SliderFloat("DiffuseK", &material.DiffuseCo, 0.0f, 1.0f);
		ImGui::SliderFloat("SpecularK", &material.SpecualarCo, 0.0f, 1.0f);
		ImGui::SliderFloat("Shininess", &material.Shininess, 2.0f, 1024.0f);
		ImGui::SliderFloat("Exposure", &colorCorrect.Exposure, 0.0f, 2.0f);
		ImGui::SliderFloat("Contrast", &colorCorrect.Contrast, 0.0f, 2.0f);
		ImGui::SliderFloat("Brightness", &colorCorrect.Brightness, 0.0f, 2.0f);
	}


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

