#pragma once
#include "Transforms.h"
#include "MathUtils.h"
namespace Hail
{
	class Camera
	{
	public:
		Camera();
		Transform3D& GetTransform() { return m_transform; }
		float GetFov() { return m_fov; }
		void SetFov(float fov) { m_fov = fov; }

		float GetFar() const { return m_far; }
		void SetFar(float far);

		float GetNear() { return m_near; }
		void SetNear(float near);

		bool GetOrthographic() { return m_orthographic; }
		void SetOrthographic(float orthoState);

		static Camera LerpCamera(const Camera cam1, const Camera cam2, float t);

	private:
		Transform3D m_transform;
		float m_fov;
		float m_far;
		float m_near;

		bool m_orthographic;
	};

	class Camera2D
	{
	public:
		Camera2D();

		const glm::uvec2& GetResolution() const { return m_screenResolution; }
		void SetResolution(glm::uvec2 resolution) { m_screenResolution = resolution; }
		//Position of the camera in pixel space
		glm::vec2 GetPosition() const { return m_position; }
		glm::vec2 GetNormalizedPosition() const { return m_normalizedPosition; }
		void SetPosition(glm::vec2 position);

		//At zoom 1.0 the pixel on the screen will be the actual pixel size, so a 1 to 1 ratio
		float GetZoom() const { return m_zoom; }
		void SetZoom(float zoom);

		//Transforms the position from a space to the 0-1 space of the camera
		void TransformToCameraSpace(Transform2D& transformToTransform) const;
		void TransformLineToCameraSpaceFromNormalizedSpace(glm::vec3& start, glm::vec3& end) const;
		void TransformLineToCameraSpaceFromPixelSpace(glm::vec3& start, glm::vec3& end) const;
	private:
		float m_zoom{};
		glm::vec2 m_position;
		glm::vec2 m_normalizedPosition;
		glm::uvec2 m_screenResolution;
	};
}
 