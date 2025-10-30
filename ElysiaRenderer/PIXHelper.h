#pragma once
#include "Helper.h"

namespace ElysiaHelper
{
	class PIXHelper
	{
    public:
        PIXHelper(ID3D12GraphicsCommandList* cmdList, const wchar_t* msg) : m_pCommand(cmdList)
        {
            PIXBeginEvent(cmdList, 0, msg);
        }

        PIXHelper(ID3D12GraphicsCommandList* cmdList, const char* msg) : m_pCommand(cmdList)
        {
            PIXBeginEvent(cmdList, 0, msg);
        }

        ~PIXHelper()
        {
            PIXEndEvent(m_pCommand);
        }

	private:
		ID3D12GraphicsCommandList* m_pCommand = nullptr;
	};
}