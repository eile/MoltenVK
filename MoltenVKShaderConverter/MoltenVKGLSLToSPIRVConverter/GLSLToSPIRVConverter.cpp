/*
 * GLSLToSPIRVConverter.cpp
 *
 * Copyright (c) 2014-2018 The Brenwill Workshop Ltd. (http://www.brenwill.com)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "GLSLToSPIRVConverter.h"
#include "MVKCommonEnvironment.h"
#include "SPIRVToMSLConverter.h"
#include "MVKStrings.h"
#include "GlslangToSpv.h"
#include "../../External/glslang/SPIRV/disassemble.h"
#include "doc.h"
#include <sstream>

using namespace std;
using namespace mvk;


#pragma mark -
#pragma mark GLSLToSPIRVConverter

/** Configures the specified limit resources structure used by the GLSL compiler. */
void configureGLSLCompilerResources(TBuiltInResource* glslCompilerResources);

/** Returns the GLSL compiler language type corresponding to the specified MoltenVK shader stage. */
EShLanguage eshLanguageFromMVKShaderStage(const MVKShaderStage mvkShaderStage);

MVK_PUBLIC_SYMBOL void GLSLToSPIRVConverter::setGLSL(const string& glslSrc) { _glsl = glslSrc; }

MVK_PUBLIC_SYMBOL const string& GLSLToSPIRVConverter::getGLSL() { return _glsl; }

MVK_PUBLIC_SYMBOL bool GLSLToSPIRVConverter::convert(MVKShaderStage shaderStage,
													 bool shouldLogGLSL,
													 bool shouldLogSPIRV) {
	_wasConverted = true;
	_resultLog.clear();
	_spirv.clear();

	if (shouldLogGLSL) { logGLSL("Converting"); }

	EShMessages messages = (EShMessages)(EShMsgDefault | EShMsgSpvRules | EShMsgVulkanRules);

	EShLanguage stage = eshLanguageFromMVKShaderStage(shaderStage);
	TBuiltInResource glslCompilerResources;
	configureGLSLCompilerResources(&glslCompilerResources);
	const char *glslStrings[1];
	glslStrings[0] = _glsl.data();

	// Create and compile a shader from the source code
	glslang::TShader glslShader(stage);
	glslShader.setStrings(glslStrings, 1);
	if (glslShader.parse(&glslCompilerResources, 100, false, messages)) {
		if (shouldLogGLSL) {
			logMsg(glslShader.getInfoLog());
			logMsg(glslShader.getInfoDebugLog());
		}
	} else {
		logError(glslShader.getInfoLog());
		logError(glslShader.getInfoDebugLog());
		return logError("Error compiling GLSL when converting GLSL to SPIR-V.");
	}

	// Create and link a shader program containing the single shader
	glslang::TProgram glslProgram;
	glslProgram.addShader(&glslShader);
	if ( !glslProgram.link(messages) ) {
		logError(glslProgram.getInfoLog());
		logError(glslProgram.getInfoDebugLog());
		return logError("Error creating GLSL program when converting GLSL to SPIR-V.");
	}

	// Output the SPIR-V code from the shader program
	glslang::GlslangToSpv(*glslProgram.getIntermediate(stage), _spirv);

	if (shouldLogSPIRV) { logSPIRV("Converted"); }

	return _wasConverted;
}

MVK_PUBLIC_SYMBOL const vector<uint32_t>& GLSLToSPIRVConverter::getSPIRV() { return _spirv; }

MVK_PUBLIC_SYMBOL bool GLSLToSPIRVConverter::getWasConverted() { return _wasConverted; }

MVK_PUBLIC_SYMBOL const string& GLSLToSPIRVConverter::getResultLog() { return _resultLog; }

/** Appends the message text to the result log. */
void GLSLToSPIRVConverter::logMsg(const char* logMsg) {
	string trimMsg = trim(logMsg);
	if ( !trimMsg.empty() ) {
		_resultLog += trimMsg;
		_resultLog += "\n\n";
	}
}

/** Appends the error text to the result log, sets the wasConverted property to false, and returns it. */
bool GLSLToSPIRVConverter::logError(const char* errMsg) {
	logMsg(errMsg);
	_wasConverted = false;
	return _wasConverted;
}

/** Appends the SPIR-V to the result log, indicating whether it is being converted or was converted. */
void GLSLToSPIRVConverter::logSPIRV(const char* opDesc) {

	string spvLog;
	mvk::logSPIRV(_spirv, spvLog);

	_resultLog += opDesc;
	_resultLog += " SPIR-V:\n";
	_resultLog += spvLog;
	_resultLog += "\nEnd SPIR-V\n\n";
}

/** Validates that the SPIR-V code will disassemble during logging. */
bool GLSLToSPIRVConverter::validateSPIRV() {
	if (_spirv.size() < 5) { return false; }
	if (_spirv[0] != spv::MagicNumber) { return false; }
	if (_spirv[4] != 0) { return false; }
	return true;
}

/** Appends the GLSL to the result log, indicating whether it is being converted or was converted. */
void GLSLToSPIRVConverter::logGLSL(const char* opDesc) {
	_resultLog += opDesc;
	_resultLog += " GLSL:\n";
	_resultLog += _glsl;
	_resultLog += "\nEnd GLSL\n\n";
}


#pragma mark -
#pragma mark Support functions

void configureGLSLCompilerResources(TBuiltInResource* glslCompilerResources) {
	glslCompilerResources->maxLights = 32;
	glslCompilerResources->maxClipPlanes = 6;
	glslCompilerResources->maxTextureUnits = 32;
	glslCompilerResources->maxTextureCoords = 32;
	glslCompilerResources->maxVertexAttribs = 64;
	glslCompilerResources->maxVertexUniformComponents = 4096;
	glslCompilerResources->maxVaryingFloats = 64;
	glslCompilerResources->maxVertexTextureImageUnits = 32;
	glslCompilerResources->maxCombinedTextureImageUnits = 80;
	glslCompilerResources->maxTextureImageUnits = 32;
	glslCompilerResources->maxFragmentUniformComponents = 4096;
	glslCompilerResources->maxDrawBuffers = 32;
	glslCompilerResources->maxVertexUniformVectors = 128;
	glslCompilerResources->maxVaryingVectors = 8;
	glslCompilerResources->maxFragmentUniformVectors = 16;
	glslCompilerResources->maxVertexOutputVectors = 16;
	glslCompilerResources->maxFragmentInputVectors = 15;
	glslCompilerResources->minProgramTexelOffset = -8;
	glslCompilerResources->maxProgramTexelOffset = 7;
	glslCompilerResources->maxClipDistances = 8;
	glslCompilerResources->maxComputeWorkGroupCountX = 65535;
	glslCompilerResources->maxComputeWorkGroupCountY = 65535;
	glslCompilerResources->maxComputeWorkGroupCountZ = 65535;
	glslCompilerResources->maxComputeWorkGroupSizeX = 1024;
	glslCompilerResources->maxComputeWorkGroupSizeY = 1024;
	glslCompilerResources->maxComputeWorkGroupSizeZ = 64;
	glslCompilerResources->maxComputeUniformComponents = 1024;
	glslCompilerResources->maxComputeTextureImageUnits = 16;
	glslCompilerResources->maxComputeImageUniforms = 8;
	glslCompilerResources->maxComputeAtomicCounters = 8;
	glslCompilerResources->maxComputeAtomicCounterBuffers = 1;
	glslCompilerResources->maxVaryingComponents = 60;
	glslCompilerResources->maxVertexOutputComponents = 64;
	glslCompilerResources->maxGeometryInputComponents = 64;
	glslCompilerResources->maxGeometryOutputComponents = 128;
	glslCompilerResources->maxFragmentInputComponents = 128;
	glslCompilerResources->maxImageUnits = 8;
	glslCompilerResources->maxCombinedImageUnitsAndFragmentOutputs = 8;
	glslCompilerResources->maxCombinedShaderOutputResources = 8;
	glslCompilerResources->maxImageSamples = 0;
	glslCompilerResources->maxVertexImageUniforms = 0;
	glslCompilerResources->maxTessControlImageUniforms = 0;
	glslCompilerResources->maxTessEvaluationImageUniforms = 0;
	glslCompilerResources->maxGeometryImageUniforms = 0;
	glslCompilerResources->maxFragmentImageUniforms = 8;
	glslCompilerResources->maxCombinedImageUniforms = 8;
	glslCompilerResources->maxGeometryTextureImageUnits = 16;
	glslCompilerResources->maxGeometryOutputVertices = 256;
	glslCompilerResources->maxGeometryTotalOutputComponents = 1024;
	glslCompilerResources->maxGeometryUniformComponents = 1024;
	glslCompilerResources->maxGeometryVaryingComponents = 64;
	glslCompilerResources->maxTessControlInputComponents = 128;
	glslCompilerResources->maxTessControlOutputComponents = 128;
	glslCompilerResources->maxTessControlTextureImageUnits = 16;
	glslCompilerResources->maxTessControlUniformComponents = 1024;
	glslCompilerResources->maxTessControlTotalOutputComponents = 4096;
	glslCompilerResources->maxTessEvaluationInputComponents = 128;
	glslCompilerResources->maxTessEvaluationOutputComponents = 128;
	glslCompilerResources->maxTessEvaluationTextureImageUnits = 16;
	glslCompilerResources->maxTessEvaluationUniformComponents = 1024;
	glslCompilerResources->maxTessPatchComponents = 120;
	glslCompilerResources->maxPatchVertices = 32;
	glslCompilerResources->maxTessGenLevel = 64;
	glslCompilerResources->maxViewports = 16;
	glslCompilerResources->maxVertexAtomicCounters = 0;
	glslCompilerResources->maxTessControlAtomicCounters = 0;
	glslCompilerResources->maxTessEvaluationAtomicCounters = 0;
	glslCompilerResources->maxGeometryAtomicCounters = 0;
	glslCompilerResources->maxFragmentAtomicCounters = 8;
	glslCompilerResources->maxCombinedAtomicCounters = 8;
	glslCompilerResources->maxAtomicCounterBindings = 1;
	glslCompilerResources->maxVertexAtomicCounterBuffers = 0;
	glslCompilerResources->maxTessControlAtomicCounterBuffers = 0;
	glslCompilerResources->maxTessEvaluationAtomicCounterBuffers = 0;
	glslCompilerResources->maxGeometryAtomicCounterBuffers = 0;
	glslCompilerResources->maxFragmentAtomicCounterBuffers = 1;
	glslCompilerResources->maxCombinedAtomicCounterBuffers = 1;
	glslCompilerResources->maxAtomicCounterBufferSize = 16384;
	glslCompilerResources->maxTransformFeedbackBuffers = 4;
	glslCompilerResources->maxTransformFeedbackInterleavedComponents = 64;
	glslCompilerResources->maxCullDistances = 8;
	glslCompilerResources->maxCombinedClipAndCullDistances = 8;
	glslCompilerResources->maxSamples = 4;
	glslCompilerResources->limits.nonInductiveForLoops = 1;
	glslCompilerResources->limits.whileLoops = 1;
	glslCompilerResources->limits.doWhileLoops = 1;
	glslCompilerResources->limits.generalUniformIndexing = 1;
	glslCompilerResources->limits.generalAttributeMatrixVectorIndexing = 1;
	glslCompilerResources->limits.generalVaryingIndexing = 1;
	glslCompilerResources->limits.generalSamplerIndexing = 1;
	glslCompilerResources->limits.generalVariableIndexing = 1;
	glslCompilerResources->limits.generalConstantMatrixVectorIndexing = 1;
}

EShLanguage eshLanguageFromMVKShaderStage(const MVKShaderStage mvkShaderStage) {
	switch (mvkShaderStage) {
		case kMVKShaderStageVertex:			return EShLangVertex;
		case kMVKShaderStageTessControl:	return EShLangTessControl;
		case kMVKShaderStageTessEval:		return EShLangTessEvaluation;
		case kMVKShaderStageGeometry:		return EShLangGeometry;
		case kMVKShaderStageFragment:		return EShLangFragment;
		case kMVKShaderStageCompute:		return EShLangCompute;
		default:							return EShLangVertex;
	}
}


#pragma mark Library initialization

/**
 * Called automatically when the framework is loaded and initialized.
 *
 * Initialize the
 */
static bool _wasShaderConverterInitialized = false;
__attribute__((constructor)) static void MVKShaderConverterInit() {
	if (_wasShaderConverterInitialized ) { return; }

	glslang::InitializeProcess();

	_wasShaderConverterInitialized = true;
}


