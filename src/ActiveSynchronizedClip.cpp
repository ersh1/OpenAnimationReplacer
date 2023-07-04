#include "ActiveSynchronizedClip.h"

#include "OpenAnimationReplacer.h"

ActiveSynchronizedClip::ActiveSynchronizedClip(RE::BSSynchronizedClipGenerator* a_synchronizedClipGenerator, const RE::hkbContext& a_context) :
	_synchronizedClipGenerator(a_synchronizedClipGenerator),
	_originalSynchronizedIndex(a_synchronizedClipGenerator->synchronizedAnimationBindingIndex)
{
	// get the offset and alter the index
	_synchronizedClipGenerator->synchronizedAnimationBindingIndex += OpenAnimationReplacer::GetSingleton().GetSynchronizedClipsIDOffset(a_context.character);
}

ActiveSynchronizedClip::~ActiveSynchronizedClip()
{
	// restore original index
	_synchronizedClipGenerator->synchronizedAnimationBindingIndex = _originalSynchronizedIndex;
}

void ActiveSynchronizedClip::OnActivate([[maybe_unused]] RE::BSSynchronizedClipGenerator* a_synchronizedClipGenerator, [[maybe_unused]] const RE::hkbContext& a_context)
{

}
