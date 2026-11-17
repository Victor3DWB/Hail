#pragma once
#include "glm\vec3.hpp"
#include "glm\vec2.hpp"
#include "glm\ext\quaternion_float.hpp"
#include "MathUtils.h"


namespace Hail
{
	class Transform3D
	{
	public:
		Transform3D() {
			m_pos = glm::vec3();
			m_scl = glm::vec3(1, 1, 1);
			m_rot = glm::vec3();
			m_quat = glm::qua<float, glm::packed_highp>();
		}
		Transform3D(const Transform3D& matrix);
		Transform3D(const glm::highp_mat4& matrix);

		const Transform3D& operator=(const Transform3D& transform);
		const Transform3D& operator=(const glm::highp_mat4& matrix);

		__forceinline glm::vec3 GetPosition() const { return m_pos; };
		__forceinline glm::vec3 GetRotation() const { return m_rot; };
		__forceinline glm::qua<float, glm::packed_highp> GetRotationQ() const { return m_quat; };
		__forceinline glm::vec3 GetScale()	const { return m_scl; };



		void LookAt(const glm::vec3 worldPositionToLookAt, const glm::vec3 fromPos);
		static glm::highp_mat4 LerpTransforms_mat(const Transform3D& tr1, const Transform3D& tr2, const float t);
		static Transform3D LerpTransforms_t(const Transform3D& tr1, const Transform3D& tr2, const float t);
		static glm::highp_mat4 GetMatrix(const Transform3D& transform);
		glm::vec3 GetForward() const;
		glm::vec3 GetRight() const;
		glm::vec3 GetUp() const;

		void SetPosition(const glm::vec3& pos);
		void AddToPosition(const glm::vec3& pos);
		void SetRotation(const glm::vec3& rot);
		void SetRotation(const glm::qua<float, glm::packed_highp>& rot);
		//Set rotation in Eueler angles
		void AddToRotation(const glm::vec3& rotEuler);
		void SetScale(const glm::vec3& scl);
		void AddToScale(const glm::vec3& scl);

	private:
		glm::vec3 m_pos, m_scl, m_rot;
		glm::qua<float, glm::packed_highp> m_quat;
	};


	class Transform2D
	{
	public:
		Transform2D() {
			m_pos = glm::vec2();
			m_scl = glm::vec2(1, 1);
			m_rot = Math::PI2f;
		}
		const Transform2D& operator=(const Transform2D& transform);

		__forceinline float GetRotationEuler() const { return m_rot * Math::RadToDegf + 90.f; };
		__forceinline float GetRotationRad() const { return m_rot; };
		__forceinline const glm::vec2& GetScale()	const { return m_scl; };
		__forceinline const glm::vec2& GetPosition() const { return m_pos; };
		__forceinline glm::vec2 GetForward() const { return glm::vec2(cos(m_rot), sin(m_rot)); }
		__forceinline glm::vec2 GetRight() const { return glm::vec2(-sin(m_rot), cos(m_rot)); }
		static const Transform2D LerpTransforms(const Transform2D& tr1, const Transform2D& tr2, const float t);

		void SetPosition(const glm::vec2 pos);
		void AddToPosition(const glm::vec2 pos);
		void SetRotationRad(const float rot);
		void SetRotationEuler(const float rot);
		void AddToRotationRad(const float rot);
		void AddToRotationEuler(const float rot);
		void SetScale(const glm::vec2 scl);
		void AddToScale(const glm::vec2 scl);
		void LookAt(const glm::vec2 normalizedDirection);

	private:
		glm::vec2 m_pos, m_scl;
		float m_rot;
	};
}
