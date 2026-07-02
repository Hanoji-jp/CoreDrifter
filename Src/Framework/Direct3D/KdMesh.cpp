#include "Framework/KdFramework.h"

#include "KdMesh.h"

#include "KdGLTFLoader.h"

#include <map>
#include <array>
#include <cmath>

//=============================================================
//
// Mesh
//
//=============================================================

void KdMesh::SetToDevice() const
{
	// 頂点バッファセット（スロット0=通常頂点、スロット1=アウトライン用スムース法線）
	// ※Lit等の入力レイアウトはスロット0しか参照しないのでスロット1は無視される。
	//   アウトライン用レイアウトのみスロット1（NORMAL1）を使う。
	if (m_hasSmoothNormal)
	{
		ID3D11Buffer* bufs[2] = { m_vertBuf.GetBuffer(), m_smoothNormalBuf.GetBuffer() };
		UINT strides[2] = { sizeof(KdMeshVertex), sizeof(Math::Vector3) };
		UINT offsets[2] = { 0, 0 };
		KdDirect3D::Instance().WorkDevContext()->IASetVertexBuffers(0, 2, bufs, strides, offsets);
	}
	else
	{
		UINT stride = sizeof(KdMeshVertex);	// 1頂点のサイズ
		UINT offset = 0;					// オフセット
		KdDirect3D::Instance().WorkDevContext()->IASetVertexBuffers(0, 1, m_vertBuf.GetAddress(), &stride, &offset);
	}

	// インデックスバッファセット
	KdDirect3D::Instance().WorkDevContext()->IASetIndexBuffer(m_indxBuf.GetBuffer(), DXGI_FORMAT_R32_UINT, 0);

	//プリミティブ・トポロジーをセット
	KdDirect3D::Instance().WorkDevContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

//=============================================================
// 生成
// 頂点配列、インデックス配列、サブセット配列（マテリアルなど）の生成
//=============================================================
bool KdMesh::Create(const std::vector<KdMeshVertex>& vertices, const std::vector<KdMeshFace>& faces, const std::vector<KdMeshSubset>& subsets, bool isSkinMesh)
{
	Release();

	//------------------------------
	// サブセット情報
	//------------------------------
	m_subsets = subsets;

	//------------------------------
	// 頂点バッファ作成
	//------------------------------
	if(vertices.size() > 0)
	{
		// 書き込むデータ
		D3D11_SUBRESOURCE_DATA initData;
		initData.pSysMem = &vertices[0];				// バッファに書き込む頂点配列の先頭アドレス
		initData.SysMemPitch = 0;
		initData.SysMemSlicePitch = 0;

		// 頂点バッファ作成
		if (FAILED(m_vertBuf.Create(D3D11_BIND_VERTEX_BUFFER, sizeof(KdMeshVertex) * (UINT)vertices.size(), D3D11_USAGE_DEFAULT, &initData)))
		{
			Release();
			return false;
		}

		// 座標のみの配列
		m_positions.resize(vertices.size());
		for (UINT i = 0; i < m_positions.size(); i++)
		{
			m_positions[i] = vertices[i].Pos;
		}

		// AA境界データ作成
		DirectX::BoundingBox::CreateFromPoints(m_aabb, m_positions.size(), &m_positions[0], sizeof(Math::Vector3));
		// 境界球データ作成
		DirectX::BoundingSphere::CreateFromPoints(m_bs, m_positions.size(), &m_positions[0], sizeof(Math::Vector3));

		//------------------------------
		// アウトライン用スムース法線の作成
		//   同じ座標の頂点（ハードエッジで分裂した法線）を「位置で溶接」して平均する。
		//   これを法線方向の押し出しに使うと、面がバラバラに離れず連続した輪郭になる。
		//------------------------------
		{
			// 位置を量子化してキー化（微小な誤差を吸収して同一頂点をまとめる）
			auto keyOf = [](const Math::Vector3& p) -> std::array<int, 3>
			{
				return { (int)std::lround(p.x * 1000.0f),
						 (int)std::lround(p.y * 1000.0f),
						 (int)std::lround(p.z * 1000.0f) };
			};

			std::map<std::array<int, 3>, Math::Vector3> accum;
			for (const auto& v : vertices)
			{
				accum[keyOf(v.Pos)] += v.Normal;
			}

			std::vector<Math::Vector3> smoothNormals(vertices.size());
			for (size_t i = 0; i < vertices.size(); ++i)
			{
				Math::Vector3 n = accum[keyOf(vertices[i].Pos)];
				if (n.LengthSquared() > 1e-12f) { n.Normalize(); }
				else                            { n = vertices[i].Normal; }
				smoothNormals[i] = n;
			}

			D3D11_SUBRESOURCE_DATA snData;
			snData.pSysMem = &smoothNormals[0];
			snData.SysMemPitch = 0;
			snData.SysMemSlicePitch = 0;
			if (SUCCEEDED(m_smoothNormalBuf.Create(D3D11_BIND_VERTEX_BUFFER,
				sizeof(Math::Vector3) * (UINT)smoothNormals.size(), D3D11_USAGE_DEFAULT, &snData)))
			{
				m_hasSmoothNormal = true;
			}
		}
	}

	//------------------------------
	// インデックスバッファ作成
	//------------------------------
	if(faces.size() > 0)
	{
		// 書き込むデータ
		D3D11_SUBRESOURCE_DATA initData;
		initData.pSysMem = &faces[0];				// バッファに書き込む頂点配列の先頭アドレス
		initData.SysMemPitch = 0;
		initData.SysMemSlicePitch = 0;

		// バッファ作成
		if (FAILED(m_indxBuf.Create(D3D11_BIND_INDEX_BUFFER, (UINT)faces.size() * sizeof(KdMeshFace), D3D11_USAGE_DEFAULT, &initData)))
		{
			Release();
			return false;
		}

		// 面情報コピー
		m_faces = faces;
	}


	m_isSkinMesh = isSkinMesh;

	return true;
}


void KdMesh::DrawSubset(int subsetNo) const
{
	// 範囲外のサブセットはスキップ
	if (subsetNo >= (int)m_subsets.size())return;
	// 面数が0なら描画スキップ
	if (m_subsets[subsetNo].FaceCount == 0)return;

	// 描画
	KdDirect3D::Instance().WorkDevContext()->DrawIndexed(m_subsets[subsetNo].FaceCount * 3, m_subsets[subsetNo].FaceStart * 3, 0);
}
