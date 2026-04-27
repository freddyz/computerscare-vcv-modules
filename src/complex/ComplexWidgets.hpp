#pragma once

#include "math/ComplexFormat.hpp"

using namespace rack;





namespace cpx {

	enum portModes {
      RECT_INTERLEAVED,
      POLAR_INTERLEAVED,
      RECT_SEPARATED,
      POLAR_SEPARATED
  };

	inline void drawArrowTo(NVGcontext* vg,math::Vec tipPosition,float baseWidth=5.f) {
		float angle = tipPosition.arg();
		float len = tipPosition.norm();

		//nvgSave(vg);
    nvgBeginPath(vg);
    nvgStrokeWidth(vg, 1.f);
    nvgStrokeColor(vg,  COLOR_COMPUTERSCARE_DARK_GREEN);
    nvgFillColor(vg,  COLOR_COMPUTERSCARE_LIGHT_GREEN);
   
		nvgRotate(vg,angle);
		nvgMoveTo(vg,0,-baseWidth);
		//nvgArcTo(vg,0,0,0,baseWidth/2,13);
		//nvgLineTo(vg,-2,-baseWidth/2);
		//nvgLineTo(vg,-2,baseWidth/2);
		nvgQuadTo(vg,baseWidth/2,0,0,baseWidth);
		nvgLineTo(vg,0,baseWidth);
		nvgLineTo(vg,len,0);
		//nvgLineTo(vg,len,-1);

 		nvgClosePath(vg);
    nvgFill(vg);
    nvgStroke(vg);

   // nvgRestore(vg);
	}


	struct ComplexXY : TransparentWidget {
	ComputerscareComplexBase* module;

	Vec clickedMousePosition;
	Vec thisPos;
	math::Vec deltaPos;
	
	Vec pixelsOrigin;
	Vec pixelsDiff;

	Vec origComplexValue;
	float origComplexLength;

	Vec newZ;

	int paramA;

	bool editing=false;

	float originalMagnituteRadiusPixels = 120.f;

	ComplexXY(ComputerscareComplexBase* mod,int indexParamA) {
		module=mod;
		paramA = indexParamA;
		//box.size = Vec(30,30);
		TransparentWidget();
	}

	void onButton(const event::Button &e) override {

		if (e.button == GLFW_MOUSE_BUTTON_LEFT) {
			if(e.action == GLFW_PRESS){
				e.consume(this);
				editing=true;
				clickedMousePosition = APP->scene->getMousePos();
				if(module) {
					float complexA = module->params[paramA].getValue();
					float complexB = module->params[paramA+1].getValue();
					origComplexValue = Vec(complexA,-complexB);
					origComplexLength=origComplexValue.norm();
					
					if(origComplexLength < 0.1) {
						origComplexLength = 1;
						pixelsOrigin = clickedMousePosition;
					} else {
						pixelsOrigin = clickedMousePosition.minus(origComplexValue.mult(originalMagnituteRadiusPixels/origComplexLength));
					}
				}
			} 
		}
	}
	void onHoverKey(const HoverKeyEvent& e) override {
		if(e.key == GLFW_KEY_ESCAPE) {
			DEBUG("escape");
		}
	}

	void onDragEnd(const event::DragEnd &e) override {
		editing=false;
	}

	void step() override {
		if(editing && module) {
			thisPos = APP->scene->getMousePos();

			//in scaled pixels

			pixelsDiff = thisPos.minus(pixelsOrigin);
			newZ = pixelsDiff.div(originalMagnituteRadiusPixels).mult(origComplexLength);

			module->params[paramA].setValue(newZ.x);
			module->params[paramA+1].setValue(-newZ.y);
		} else {
			if(module) {
				newZ = Vec(module->params[paramA].getValue(),-module->params[paramA+1].getValue());
			} else {
				newZ = Vec(-10 + 20* random::uniform(),-10 + 20* random::uniform());
			}
		}
	}



	void draw(const DrawArgs &args) override {
		float fullR = box.size.x/2;

		//circle at complex radius 1
		nvgSave(args.vg);
		//nvgResetTransform(args.vg);
			nvgTranslate(args.vg,fullR,fullR);
	      nvgBeginPath(args.vg);
	      nvgStrokeWidth(args.vg, 2.f);
	      nvgFillColor(args.vg,  nvgRGBA(0, 10, 30,50));
	      //nvgMoveTo(args.vg,box.size.x/2,box.size.y/2);
	      nvgEllipse(args.vg, 0,0,fullR,fullR);
	      nvgClosePath(args.vg);
	      nvgFill(args.vg);

   			nvgBeginPath(args.vg);
	      nvgStrokeWidth(args.vg, 2.f);
	      nvgStrokeColor(args.vg,  nvgRGB(40, 110, 80));
	      nvgMoveTo(args.vg, 0,0);


	      float length=newZ.norm();
	      Vec tip = newZ.normalize().mult(2*fullR/M_PI*std::atan(length));

	      drawArrowTo(args.vg,tip,box.size.x/7);
	      nvgRestore(args.vg);

	}
	void drawLayer(const DrawArgs &args,int layer) override {

		
		if(layer==1) {
			if(editing) {

				float pxRatio = APP->window->pixelRatio;

				nvgSave(args.vg);
				//reset to "undo" the zoom
				nvgResetTransform(args.vg);
				nvgScale(args.vg, pxRatio, pxRatio);

				nvgTranslate(args.vg,pixelsOrigin.x,pixelsOrigin.y);

				//circle at complex radius 1
	      nvgBeginPath(args.vg);
	      nvgStrokeWidth(args.vg, 3.f);
	      nvgFillColor(args.vg,  nvgRGBA(0, 10, 30,60));
	      nvgEllipse(args.vg, 0, 0,originalMagnituteRadiusPixels/origComplexLength, originalMagnituteRadiusPixels/origComplexLength);
	      nvgClosePath(args.vg);
	      nvgFill(args.vg);

				//circle at the zero point
				nvgBeginPath(args.vg);
	      nvgStrokeWidth(args.vg, 3.f);
	      nvgFillColor(args.vg,  nvgRGB(249, 220, 214));
	      nvgEllipse(args.vg, 0, 0,10.f, 10.f);
	      nvgClosePath(args.vg);
	      nvgFill(args.vg);


				//circle at the current complex value
				nvgBeginPath(args.vg);
	      nvgStrokeWidth(args.vg, 3.f);
	      nvgStrokeColor(args.vg,  nvgRGB(0, 100, 200));
	      nvgEllipse(args.vg, 0, 0,originalMagnituteRadiusPixels, originalMagnituteRadiusPixels);
	      nvgClosePath(args.vg);
	      nvgStroke(args.vg);

	      

	      //circle at complex radius 10
	      nvgBeginPath(args.vg);
	      nvgStrokeWidth(args.vg, 3.f);
	      nvgStrokeColor(args.vg,  nvgRGB(240, 30, 51));
	      nvgEllipse(args.vg, 0, 0,10.f*originalMagnituteRadiusPixels/origComplexLength, 10.f*originalMagnituteRadiusPixels/origComplexLength);
	      nvgClosePath(args.vg);
	      nvgStroke(args.vg);


	     

	      //line from the zero point to the original complex number
	      Vec originalComplexPixels =origComplexValue.mult(originalMagnituteRadiusPixels/origComplexLength);
	      nvgBeginPath(args.vg);
	      nvgStrokeWidth(args.vg, 5.f);
	      nvgStrokeColor(args.vg,  nvgRGB(140, 120, 80));
	      nvgMoveTo(args.vg, 0,0);
	      nvgLineTo(args.vg,originalComplexPixels.x,originalComplexPixels.y);
	     	nvgClosePath(args.vg);
	      nvgStroke(args.vg);

	      //line from the zero point to the users mouse
	      
	      drawArrowTo(args.vg,pixelsDiff);
	     	
	    
	      nvgRestore(args.vg);
			} 
		}
		Widget::drawLayer(args,layer);
	}
};

	struct CompolyModeParam : ParamQuantity {
		std::string getDisplayValueString() override {
			int mode = getValue();
			if(mode == RECT_INTERLEAVED) {
				return "Rectangular Interleaved";
			} else if(mode == POLAR_INTERLEAVED) {
				return "Polar Interleaved";
			}else if(mode == RECT_SEPARATED) {
				return "Rectangular Separated";
			}else if(mode == POLAR_SEPARATED) {
				return "Polar Separated";
			}
			return "";
		}
	};

	template <int TModeParamIndex,int blockNum=0>
	struct CompolyPortInfo : PortInfo {

		std::string getName() override{
			return name + ", " + getModeName();
		}
		std::string getModeName() {
			if(module) {
				int mode = module->params[TModeParamIndex].getValue();
				if(mode == RECT_INTERLEAVED) {
					return blockNum==0 ? "x,y #1-8" : "x,y #9-16";
				} else if(mode == POLAR_INTERLEAVED) {
					return blockNum==0 ? "r,θ #1-8" : "r,θ #9-16";
				}else if(mode == RECT_SEPARATED) {
					return blockNum==0 ? "x" : "y";
				}else if(mode == POLAR_SEPARATED) {
					return blockNum==0 ? "r" : "θ";
				}
			}
			return "";
		}
	};

	struct ComplexOutport : ComputerscareSvgPort {
		ComplexOutport() {
			//setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/complex-outjack-skewR.svg")));
		}
	};

	struct CompolyTypeLabelSwitch : app::SvgSwitch {
		widget::TransformWidget* tw;
		CompolyTypeLabelSwitch() {
			shadow->opacity = 0.f;
			addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/complex-labels/xy.svg")));
			addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/complex-labels/rtheta.svg")));
			addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/complex-labels/x.svg")));
			addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/complex-labels/r.svg")));
		}
	};

	struct CompolySingleLabelSwitch : app::SvgSwitch {
		widget::TransformWidget* tw;
		//SvgWidget* svg;
		CompolySingleLabelSwitch(std::string svgLabelFilename="z") {
			shadow->opacity = 0.f;
			addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/complex-labels/"+svgLabelFilename+".svg")));
		}
	};

	struct ScaledSvgWidget : TransformWidget {
		SvgWidget* svg;
		ScaledSvgWidget(float scale) {
			TransformWidget* tw = new TransformWidget();
			tw->scale(scale);
			svg = new SvgWidget();
			
			tw->addChild(svg);

			addChild(tw);
		}
		void setSVG(std::string path) {
			svg->setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, path)));
		}
	};




template <typename BASE>
struct CompolyInOrOutWidget : Widget {
	ComputerscareComplexBase* module;
	int paramID;
	int numPorts = 2;
	int lastOutMode = -1;
	std::vector<BASE*> ports;
	
	ScaledSvgWidget* leftLabel;
	ScaledSvgWidget* rightLabel;


	CompolyInOrOutWidget(math::Vec pos) {

		leftLabel = new ScaledSvgWidget(0.5);
		leftLabel->box.pos = pos.minus(Vec(0,10));
		leftLabel->setSVG("res/complex-labels/r.svg");


		rightLabel = new ScaledSvgWidget(0.5);
		rightLabel->box.pos = pos.plus(Vec(50,-10));
		rightLabel->setSVG("res/complex-labels/r.svg");

		addChild(leftLabel);
		addChild(rightLabel);
	}

	void setLabels(std::string leftFilename,std:: string rightFilename) {
		leftLabel->setSVG("res/complex-labels/"+leftFilename);
		rightLabel->setSVG("res/complex-labels/"+rightFilename);
		rightLabel->visible=true;
	}
	void setLabels(std::string leftFilename) {
		leftLabel->setSVG("res/complex-labels/"+leftFilename);
		rightLabel->visible=false;
	}
	void setPorts(std::string leftPortFilename,std::string rightPortFilename) {
		ports[0]->setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/"+leftPortFilename)));
		ports[1]->setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/"+rightPortFilename)));
	}

	void step() {
		if(module) {
			int outMode = module->params[paramID].getValue();

			if(lastOutMode != outMode) {
				lastOutMode = outMode;
						if(ports[0] && ports[1]) {
							if(outMode==RECT_INTERLEAVED) {
								setPorts("complex-outjack-skewL.svg","complex-outjack-skewL.svg");
								setLabels("xy.svg");
							} else if(outMode==POLAR_INTERLEAVED) {
								setPorts("complex-outjack-slantL.svg","complex-outjack-slantL.svg");
								setLabels("rtheta.svg");
							} else if(outMode==RECT_SEPARATED) {
								setPorts("complex-outjack-skewL.svg","complex-outjack-skewR.svg");
								setLabels("x.svg","yy.svg");
							} else if(outMode==POLAR_SEPARATED) {
								setPorts("complex-outjack-slantR.svg","complex-outjack-slantL.svg");
								setLabels("r.svg","theta.svg");
							}
						}
					}
			} else {
				setPorts("complex-outjack-skewL.svg","complex-outjack-skewL.svg");
				setLabels("xy.svg");
			}
			
		Widget::step();
	}
};

	
	struct CompolyPortsWidget : CompolyInOrOutWidget<ComplexOutport> {
		ComplexOutport* port;
		CompolySingleLabelSwitch* compolyLabel;
		Vec labelOffset;
		
		CompolyPortsWidget(math::Vec pos,ComputerscareComplexBase *cModule, int firstPortID,int compolyTypeParamID,float scale=1.0,bool isOutput=true,std::string labelSvgFilename="z") : CompolyInOrOutWidget(pos) {

			module=cModule;
			paramID = compolyTypeParamID;

			compolyLabel = new CompolySingleLabelSwitch(labelSvgFilename);

			compolyLabel->app::ParamWidget::module = cModule;
			compolyLabel->app::ParamWidget::paramId = compolyTypeParamID;
			compolyLabel->initParamQuantity();


			TransformWidget* tw = new TransformWidget();
			tw->box.pos = pos.minus(Vec(40,0));
			tw->scale(scale);

			tw->addChild(compolyLabel);
			addChild(tw);

			ports.resize(numPorts);

			for(int i = 0; i < numPorts; i++) {
				math::Vec myPos = pos.plus(Vec(30*i,0));
				if(isOutput) {
					port = createOutput<ComplexOutport>(myPos,cModule,firstPortID+i);
				}
				else {
					port = createInput<ComplexOutport>(myPos,cModule,firstPortID+i);
				}
				ports[i] = port;
				addChild(port);
			}
		}

	};

// ─── ComplexDisplayWidget ────────────────────────────────────────────────────
// Renders a complex number as styled text.  Number tokens are drawn in
// normalColor, operator tokens in dimColor, and the imaginary unit / angle
// symbol token in accentColor.  The SVG glyphs for "i" and "e" (from
// res/complex-labels/) are composed as scaled child widgets positioned each
// frame to sit inline with the surrounding text.

struct ComplexDisplayWidget : Widget {
	Module* module = nullptr;
	int paramX = 0;
	int paramY = 0;

	enum class DisplayMode { Rect, Polar };
	DisplayMode displayMode = DisplayMode::Rect;
	cpx::complex_math::AngleUnit angleUnit = cpx::complex_math::AngleUnit::Degree;
	cpx::complex_math::PolarDisplayStyle polarStyle =
		cpx::complex_math::PolarDisplayStyle::Engineering;
	int decimals = -1;

	NVGcolor normalColor = nvgRGB(0xe0, 0xe0, 0xe0);
	NVGcolor dimColor    = nvgRGB(0x78, 0x78, 0x78);
	NVGcolor accentColor = COLOR_COMPUTERSCARE_LIGHT_GREEN;

	// SVG glyph children — positioned each frame in draw()
	ScaledSvgWidget* iGlyph = nullptr;
	ScaledSvgWidget* eGlyph = nullptr;

	ComplexDisplayWidget() {
		iGlyph = new ScaledSvgWidget(0.5f);
		iGlyph->setSVG("res/complex-labels/i.svg");
		iGlyph->visible = false;
		addChild(iGlyph);

		eGlyph = new ScaledSvgWidget(0.5f);
		eGlyph->setSVG("res/complex-labels/e.svg");
		eGlyph->visible = false;
		addChild(eGlyph);
	}

	void draw(const DrawArgs& args) override {
		if (!module) {
			iGlyph->visible = false;
			eGlyph->visible = false;
			Widget::draw(args);
			return;
		}

		auto font = APP->window->loadFont(
			asset::plugin(pluginInstance, "res/fonts/Oswald-Regular.ttf"));
		if (!font) {
			Widget::draw(args);
			return;
		}

		float vx = module->params[paramX].getValue();
		float vy = module->params[paramY].getValue();

		cpx::complex_math::ComplexRenderParts parts;
		bool polar = (displayMode == DisplayMode::Polar);
		if (polar) {
			parts = cpx::complex_math::polarParts(vx, vy, angleUnit, polarStyle, decimals);
		} else {
			parts = cpx::complex_math::rectParts(vx, vy, decimals);
		}

		float fsize = box.size.y * 0.72f;
		float midY  = box.size.y * 0.55f;
		float cursorX = 1.f;
		float bounds[4];

		nvgFontFaceId(args.vg, font->handle);
		nvgFontSize(args.vg, fsize);
		nvgTextLetterSpacing(args.vg, 0.3f);
		nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);

		auto drawToken = [&](const std::string& s, NVGcolor color) {
			if (s.empty()) return;
			nvgFillColor(args.vg, color);
			nvgText(args.vg, cursorX, midY, s.c_str(), nullptr);
			nvgTextBounds(args.vg, cursorX, midY, s.c_str(), nullptr, bounds);
			cursorX = bounds[2] + 1.f;
		};

		// Glyph sizing: scale svg children to match the text cap-height.
		// i.svg viewBox height is 9.2, e.svg viewBox height is 7.0.
		// At scale 0.5 the natural mm->px mapping gives ~17px for i, ~13px for e
		// at typical rack DPI.  We just position them; ScaledSvgWidget handles scale.
		float glyphH = fsize * 0.85f;
		float iW = glyphH * 0.35f;  // i is narrow
		float eW = glyphH * 0.80f;

		iGlyph->visible = false;
		eGlyph->visible = false;

		if (!polar) {
			// real [sign imag] i
			drawToken(parts.real, normalColor);
			drawToken(" " + parts.sign + " ", dimColor);
			drawToken(parts.imag, normalColor);
			// place i glyph inline
			iGlyph->box.pos = Vec(cursorX, midY - glyphH);
			iGlyph->box.size = Vec(iW, glyphH);
			iGlyph->visible = true;
		} else if (polarStyle == cpx::complex_math::PolarDisplayStyle::Engineering) {
			// magnitude ∠ angle
			drawToken(parts.magnitude, normalColor);
			drawToken(" " + parts.angleSym + " ", accentColor);
			drawToken(parts.angle, normalColor);
		} else {
			// magnitude e^(i· angle)
			drawToken(parts.magnitude, normalColor);
			// e glyph
			eGlyph->box.pos = Vec(cursorX, midY - glyphH);
			eGlyph->box.size = Vec(eW, glyphH);
			eGlyph->visible = true;
			cursorX += eW + 1.f;
			drawToken("^(", dimColor);
			// i glyph
			iGlyph->box.pos = Vec(cursorX, midY - glyphH);
			iGlyph->box.size = Vec(iW, glyphH);
			iGlyph->visible = true;
			cursorX += iW + 1.f;
			drawToken("·" + parts.angle + ")", dimColor);
		}

		Widget::draw(args);
	}
};

} // namespace cpx
