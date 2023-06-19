#include "ActiveSynchronizedClip.h"

#include "OpenAnimationReplacer.h"

ActiveSynchronizedClip::ActiveSynchronizedClip(RE::BSSynchronizedClipGenerator* a_synchronizedClipGenerator, const RE::hkbContext& a_context) :
	_synchronizedClipGenerator(a_synchronizedClipGenerator),
	_originalSynchronizedIndex(a_synchronizedClipGenerator->synchronizedAnimIndex)
{
	// get the offset and alter the index
	_synchronizedClipGenerator->synchronizedAnimIndex += OpenAnimationReplacer::GetSingleton().GetSynchronizedClipsIDOffset(a_context.character);
}

ActiveSynchronizedClip::~ActiveSynchronizedClip()
{
	// restore original index
	_synchronizedClipGenerator->synchronizedAnimIndex = _originalSynchronizedIndex;
}
