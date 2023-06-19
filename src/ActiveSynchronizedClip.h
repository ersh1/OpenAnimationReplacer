#pragma once

// currently just a helper RAII class for synchronized clip index replacements
class ActiveSynchronizedClip
{
public:
	[[nodiscard]] ActiveSynchronizedClip(RE::BSSynchronizedClipGenerator* a_synchronizedClipGenerator, const RE::hkbContext& a_context);
	~ActiveSynchronizedClip();

protected:
	RE::BSSynchronizedClipGenerator* _synchronizedClipGenerator;
	const uint16_t _originalSynchronizedIndex;
};
