#pragma once
#include "hiface/client.hpp"
#include "hiface/database.hpp"
namespace hiface_demo {
using hiface::Image; using hiface::CameraFrame; using hiface::Feature;
using hiface::SearchOptions; using hiface::FlowKind; using hiface::FlowOptions;
using hiface::DetailedFace; using hiface::CaptureResult; using hiface::FlowResult;
using hiface::Person; using Gallery = hiface::Database; using SdkError = hiface::Error;
using hiface::check; using hiface::flowConfig;
using hiface::encodeFeature; using hiface::decodeFeature; using hiface::compareFeatureText;
}
