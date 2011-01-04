/*
 * Xournal++
 *
 * Saves a document
 *
 * @author Xournal Team
 * http://xournal.sf.net
 *
 * @license GPL
 */

#include "../model/Stroke.h"
#include "../model/Text.h"
#include "SaveHandlerHelper.h"
#include "../model/Document.h"
#include "SaveHandler.h"

SaveHandler::SaveHandler() {
	root = NULL;
	firstPdfPageVisited = false;
}

SaveHandler::~SaveHandler() {
	delete root;
}

void SaveHandler::prepareSave(Document * doc) {
	if (root) {
		// cleanup old data
		delete root;
		root = NULL;
	}
	this->firstPdfPageVisited = false;

	root = new XmlNode("xournal");
	root->setAttrib("version", "0.4.5");
	root->setAttrib("extended", "true");

	root->addChild(new XmlTextNode("title", "Xournal document - see http://math.mit.edu/~auroux/software/xournal/"));

	for (int i = 0; i < doc->getPageCount(); i++) {
		XojPage * p = doc->getPage(i);
		visitPage(root, p, doc);
	}
}

String SaveHandler::getColorStr(int c) {
	char * str = g_strdup_printf("#%08x", c << 8 | 0xff);
	String color = str;
	g_free(str);
	return color;
}

String SaveHandler::getSolidBgStr(BackgroundType type) {
	switch (type) {
	case BACKGROUND_TYPE_NONE:
		return "plain";
	case BACKGROUND_TYPE_LINED:
		return "lined";
	case BACKGROUND_TYPE_RULED:
		return "ruled";
	case BACKGROUND_TYPE_GRAPH:
		return "graph";
	}
	return "plain";
}

void SaveHandler::visitLayer(XmlNode * page, Layer * l) {
	XmlNode * layer = new XmlNode("layer");
	page->addChild(layer);
	ListIterator<Element *> it = l->elementIterator();
	while (it.hasNext()) {
		Element * e = it.next();

		if (e->getType() == ELEMENT_STROKE) {
			Stroke * s = (Stroke *) e;
			XmlPointNode * stroke = new XmlPointNode("stroke");
			layer->addChild(stroke);

			StrokeTool t = s->getToolType();

			if (t == STROKE_TOOL_PEN) {
				stroke->setAttrib("tool", "pen");
			} else if (t == STROKE_TOOL_ERASER) {
				stroke->setAttrib("tool", "eraser");
			} else if (t == STROKE_TOOL_HIGHLIGHTER) {
				stroke->setAttrib("tool", "highlighter");
			} else {
				g_warning("Unknown stroke tool type: %i", t);
				stroke->setAttrib("tool", "pen");
			}

			stroke->setAttrib("color", getColorStr(s->getColor()));

			int width = s->getWidth() * 100;
			bool hasPresureSensitivity = false;
			ArrayIterator<double> it = s->widthIterator();
			while (it.hasNext()) {
				double d = it.next();
				if ((int) (d * 100) != width) {
					hasPresureSensitivity = true;
					break;
				}
			}

			if (hasPresureSensitivity) {
				int count = s->getWidthCount() + 1;
				double * values = new double[count];
				values[0] = s->getWidth();
				const double * source = s->getWidths();
				for (int i = 1; i < count; i++) {
					values[i] = source[i - 1];
				}

				stroke->setAttrib("width", values, count);
			} else {
				stroke->setAttrib("width", s->getWidth());
			}

			int c = s->getPointCount();
			Point * points = new Point[c];
			for (int i = 0; i < c; i++) {
				points[i] = s->getPoint(i);
			}

			stroke->setPoints(points, c);
		} else if (e->getType() == ELEMENT_TEXT) {
			Text * t = (Text *) e;
			XmlTextNode * text = new XmlTextNode("text", t->getText());
			layer->addChild(text);

			XFont & f = t->getFont();

			PangoFontDescription * desc = pango_font_description_new();
			pango_font_description_set_family(desc, f.getName().c_str());
			pango_font_description_set_style(desc, f.isItalic() ? PANGO_STYLE_ITALIC : PANGO_STYLE_NORMAL);
			pango_font_description_set_weight(desc, f.isBold() ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL);
			char * sFont = pango_font_description_to_string(desc);

			text->setAttrib("font", sFont);

			pango_font_description_free(desc);
			g_free(sFont);

			text->setAttrib("size", t->getFont().getSize());
			text->setAttrib("x", t->getX());
			text->setAttrib("y", t->getY());
			text->setAttrib("color", getColorStr(t->getColor()));
		}
	}
}

void SaveHandler::visitPage(XmlNode * root, XojPage * p, Document * doc) {
	XmlNode * page = new XmlNode("page");
	root->addChild(page);
	page->setAttrib("width", p->getWidth());
	page->setAttrib("height", p->getHeight());

	XmlNode * background = new XmlNode("background");
	page->addChild(background);

	switch (p->getBackgroundType()) {
	case BACKGROUND_TYPE_PDF:

		/**
		 * ATTENTION! The original xournal can only read the XML if the attributes are in the right order!
		 * DO NOT CHANGE THE ORDER OF THE ATTRIBUTES!
		 */

		background->setAttrib("type", "pdf");
		if (!firstPdfPageVisited) {
			firstPdfPageVisited = true;
			background->setAttrib("domain", "absolute");
			String pdfName = doc->getPdfFilename();
			if (pdfName.startsWith("file://")) {
				pdfName = pdfName.substring(7);
			} else {
				// TODO: Copy the PDF to te target directory
			}
			background->setAttrib("filename", pdfName);
		}
		background->setAttrib("pageno", p->getPdfPageNr() + 1);
		break;
	case BACKGROUND_TYPE_NONE:
	case BACKGROUND_TYPE_LINED:
	case BACKGROUND_TYPE_RULED:
	case BACKGROUND_TYPE_GRAPH:
		background->setAttrib("type", "solid");
		background->setAttrib("color", getColorStr(p->getBackgroundColor()));
		background->setAttrib("style", getSolidBgStr(p->getBackgroundType()));
		break;
	case BACKGROUND_TYPE_IMAGE:
		// TODO: impelement
		break;
	}

	ListIterator<Layer*> it = p->layerIterator();
	while (it.hasNext()) {
		visitLayer(page, it.next());
	}
}

void SaveHandler::saveTo(OutputStream * out) {
	out->write("<?xml version=\"1.0\" standalone=\"no\"?>\n");
	root->writeOut(out);
}
