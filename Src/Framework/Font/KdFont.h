#pragma once

//=====================================================================
// �����̃f�[�^
//
// 1�����̃f�[�^
//=====================================================================
struct KdFontData
{
	std::shared_ptr<KdTexture>	FontTex	= nullptr;	// �����e�N�X�`��
	char						Bytes		= 1;	// 1:1�o�C�g���� 2:2�o�C�g����
	uint16_t					Code		= 0;	// �����R�[�h

	// �e�N�X�`���̎g�p�e�ʎ擾
	UINT DEBUG_GetBytes()
	{
		if (FontTex)
		{
			return FontTex->GetInfo().Width * FontTex->GetInfo().Height * 4;
		}
		return 0;
	}
};

//=====================================================================
// �t�H���g�X�v���C�g�N���X
//
// ��������e�N�X�`���Ƃ��Đ������邱�Ƃ��ł���
//=====================================================================
class KdFontSprite{
	struct TexData;

public:

	KdFontSprite() {}
	~KdFontSprite() { Release(); }

	// �������X�g�擾
	std::vector<std::shared_ptr<KdFontData>>&	GetTexList()	{ return m_TexList;		}
	// ������̑������擾
	int											GetTotalWidth()	{ return m_TotalWidth;	}
	// ������擾
	std::string									GetString()		{ return m_String;		}

	//------------------------------------------------------------
	// ���
	//------------------------------------------------------------
	void Release()
	{
		m_TexList.clear();

		m_TotalWidth	= 0;
		m_String		= "";
	}

	//------------------------------------------------------------
	// �����񂩂�t�H���g�e�N�X�`�����X�g���쐬
	// hdc			�c font���ݒ肳��Ă���DC���w��B����font���g�p�����B
	// antiAliasing	�c �A���`�G�C���A�V���O�t���O 0:�Ȃ� 1:2x 2:4x 3:8x
	// pFontDataMap	�c �쐬�����t�H���g�f�[�^�̋L��/�擾���s��Map�Bnullptr���Ǝg�p���Ȃ�(�K���e�N�X�`���쐬�ƂȂ�)
	// bAdd			�c �f�[�^��ǉ�����H
	//------------------------------------------------------------
	void CreateFontTexture(HDC hdc, const std::string& text, int antiAliasing = 0, std::array<std::shared_ptr<KdFontData>, 65536>* pFontDataArray = nullptr, bool bAdd = false);

private:

	std::vector<std::shared_ptr<KdFontData>>	m_TexList{};		// �e�N�X�`�����X�g
	std::string									m_String{};			// ������
	int											m_TotalWidth = 0;	// ������̑���
};

//=====================================================================
// �t�H���g�Ǘ��}�l�[�W���N���X
//
// �t�H���g�̓ǂݍ��݂�AKdFontSprite�̐������s��
// �t�H���g�̍쐬�𖈉�s���Ƃ��Ȃ菈�����x���Ȃ�̂ŁA
// ��x�쐬�����e�N�X�`�����g���񂹂�悤�ɂ��Ă��܂�
//=====================================================================
class KdFontManager
{
public:

	//------------------------------------------------------------
	// �����ݒ�
	//[in] hWnd		�c �E�B���h�E�̃n���h��
	//------------------------------------------------------------
	void Init(HWND hWnd);

	//------------------------------------------------------------
	// ���
	//------------------------------------------------------------
	void Release();

	//------------------------------------------------------------
	// �w�薼�̃t�H���g���A�w��ԍ��֓o�^����
	//[in] fontNo		�c �o�^����Index�ԍ�
	//[in] fontName		�c �ǉ�����t�H���g��
	//[in] h			�c �t�H���g�̑傫��(����)
	//------------------------------------------------------------
	void AddFont(int fontNo, const std::string& fontName, int h);

	//------------------------------------------------------------
	// �w��ԍ��̃t�H���g���g�p���A��������t�H���g�e�N�X�`���Ƃ��Đ�������
	//[in] fontNo			�c �g�p����o�^�t�H���g��Index�ԍ�
	//[in] text				�c ������
	//[in] antiAliasingFlag	�c �A���`�G�C���A�V���O���s�����H 0:�Ȃ� 1:2x 2:4x 3:8x
	//return �������ꂽ�e�N�X�`�����
	//------------------------------------------------------------
	std::shared_ptr<KdFontSprite> CreateFontTexture(int fontNo, const std::string& text, int antiAliasingFlag);

	//------------------------------------------------------------
	// ttf�t�@�C����ǂݍ��݁A���\�[�X�Ƃ��ēo�^����
	//------------------------------------------------------------
	void AddFontResource(const std::string& ttfFileName);

	//------------------------------------------------------------
	// �o�^����ttf�t�H���g��S�ĉ�������
	//------------------------------------------------------------
	void RemoveAllFontResource();

	// �w��ԍ��̃t�H���g���g�p���Ă��鑍�o�C�g���擾
	UINT DEBUG_GetFontBytes(int fontNo)
	{
		UINT totalByes = 0;
		auto& node = m_FontTbl[fontNo].CreatedFontDataTbl;
		for (auto& n : node)
		{
			totalByes += n->DEBUG_GetBytes();
		}
		return totalByes;
	}
	HFONT								m_hFont;
private:

	// �t�H���g�o�^�f�[�^
	struct FontData
	{
		HFONT hFont;														// �t�H���g�n���h��
		std::array<std::shared_ptr<KdFontData>, 65536>	CreatedFontDataTbl;	// �쐬�ς݃t�H���g�f�[�^�z��
	};
	std::array<FontData, 16>			m_FontTbl;							// �o�^���ꂽ�t�H���g�̔z��

	HWND								m_hWnd = 0;							// �E�B���h�E�n���h��
	HDC									m_hDC = 0;							// �f�o�C�X�R���e�L�X�g
	
	std::map<std::string, int>			m_LoadedFontMap;					// �ǉ�����ttf�t�@�C���̃��X�g

	//=====================================================
	// �V���O���g���p�^�[��
	//=====================================================
private:
	KdFontManager()		{}
	~KdFontManager()	{ Release(); }

public:
	static KdFontManager& Instance()
	{
		static KdFontManager Instance;
		return Instance;
	}
};
