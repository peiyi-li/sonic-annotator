/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Annotator
    A utility for batch feature extraction from audio files.
    Mark Levy, Chris Sutton and Chris Cannam, Queen Mary, University of London.
    Copyright 2007-2014 QMUL.

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "JAMSFeatureWriter.h"

using namespace std;
using Vamp::Plugin;
using Vamp::PluginBase;

#include "base/Exceptions.h"
#include "rdf/PluginRDFIndexer.h"

JAMSFeatureWriter::JAMSFeatureWriter() :
    FileFeatureWriter(SupportOneFilePerTrackTransform |
                      SupportOneFilePerTrack |
		      SupportStdOut,
                      "json"),
    m_network(false),
    m_networkRetrieved(false)
{
}

JAMSFeatureWriter::~JAMSFeatureWriter()
{
}

string
JAMSFeatureWriter::getDescription() const
{
    return "Write features to JSON files in JAMS (JSON Annotated Music Specification) format.";
}

JAMSFeatureWriter::ParameterList
JAMSFeatureWriter::getSupportedParameters() const
{
    ParameterList pl = FileFeatureWriter::getSupportedParameters();
    Parameter p;

    p.name = "network";
    p.description = "Attempt to retrieve RDF descriptions of plugins from network, if not available locally";
    p.hasArg = false;
    pl.push_back(p);

    return pl;
}

void
JAMSFeatureWriter::setParameters(map<string, string> &params)
{
    FileFeatureWriter::setParameters(params);

    for (map<string, string>::iterator i = params.begin();
         i != params.end(); ++i) {
        if (i->first == "network") {
            m_network = true;
        }
    }
}

void
JAMSFeatureWriter::setTrackMetadata(QString trackId, TrackMetadata metadata)
{
    QString json
	("'file_metadata':"
	 "  { 'artist': \"%1\","
	 "    'title': \"%2\" }");
    m_metadata[trackId] = json.arg(metadata.maker).arg(metadata.title);
}

void
JAMSFeatureWriter::write(QString trackId,
			 const Transform &transform,
			 const Plugin::OutputDescriptor& ,
			 const Plugin::FeatureList& features,
			 std::string /* summaryType */)
{
    QString transformId = transform.getIdentifier();

    QTextStream *sptr = getOutputStream(trackId, transformId);
    if (!sptr) {
        throw FailedToOpenOutputStream(trackId, transformId);
    }

    QTextStream &stream = *sptr;

    if (m_startedTransforms.find(transformId) == m_startedTransforms.end()) {

	identifyTask(transform);

	if (m_manyFiles ||
	    (m_startedTracks.find(trackId) == m_startedTracks.end())) {

	    // track-level preamble
	    stream << "{" << m_metadata[trackId] << endl;
	}

	stream << "'" << getTaskKey(m_tasks[transformId]) << "':" << endl;
	stream << "  [ ";
    }

    m_startedTracks.insert(trackId);
    m_startedTransforms.insert(transformId);

    for (int i = 0; i < int(features.size()); ++i) {
	
    }	
}

void
JAMSFeatureWriter::loadRDFDescription(const Transform &transform)
{
    QString pluginId = transform.getPluginIdentifier();
    if (m_rdfDescriptions.find(pluginId) != m_rdfDescriptions.end()) return;

    if (m_network && !m_networkRetrieved) {
	PluginRDFIndexer::getInstance()->indexConfiguredURLs();
	m_networkRetrieved = true;
    }

    m_rdfDescriptions[pluginId] = PluginRDFDescription(pluginId);
    
    if (m_rdfDescriptions[pluginId].haveDescription()) {
	cerr << "NOTE: Have RDF description for plugin ID \""
	     << pluginId << "\"" << endl;
    } else {
	cerr << "NOTE: No RDF description for plugin ID \""
	     << pluginId << "\"" << endl;
	if (!m_network) {
	    cerr << "      Consider using the --json-network option to retrieve plugin descriptions"  << endl;
	    cerr << "      from the network where possible." << endl;
	}
    }
}

void
JAMSFeatureWriter::identifyTask(const Transform &transform)
{
    QString transformId = transform.getIdentifier();
    if (m_tasks.find(transformId) != m_tasks.end()) return;

    loadRDFDescription(transform);
    
    Task task = UnknownTask;

    QString pluginId = transform.getPluginIdentifier();
    QString outputId = transform.getOutput();

    const PluginRDFDescription &desc = m_rdfDescriptions[pluginId];
    
    if (desc.haveDescription()) {

	PluginRDFDescription::OutputDisposition disp = 
	    desc.getOutputDisposition(outputId);

	QString af = "http://purl.org/ontology/af/";
	    
	if (disp == PluginRDFDescription::OutputSparse) {

	    QString eventUri = desc.getOutputEventTypeURI(outputId);

	    //!!! todo: allow user to prod writer for task type

	    if (eventUri == af + "Note") {
		task = NoteTask;
	    } else if (eventUri == af + "Beat") {
		task = BeatTask;
	    } else if (eventUri == af + "ChordSegment") {
		task = ChordTask;
	    } else if (eventUri == af + "KeyChange") {
		task = KeyTask;
	    } else if (eventUri == af + "KeySegment") {
		task = KeyTask;
	    } else if (eventUri == af + "Onset") {
		task = OnsetTask;
	    } else if (eventUri == af + "NonTonalOnset") {
		task = OnsetTask;
	    } else if (eventUri == af + "Segment") {
		task = SegmentTask;
	    } else if (eventUri == af + "SpeechSegment") {
		task = SegmentTask;
	    } else if (eventUri == af + "StructuralSegment") {
		task = SegmentTask;
	    } else {
		cerr << "WARNING: Unsupported event type URI <" 
		     << eventUri << ">, proceeding with UnknownTask type"
		     << endl;
	    }

	} else {

	    cerr << "WARNING: Cannot currently write dense or track-level outputs to JSON format (only sparse ones). Will proceed using UnknownTask type, but this probably isn't going to work" << endl;
	}
    }	    

    m_tasks[transformId] = task;
}

QString
JAMSFeatureWriter::getTaskKey(Task task) 
{
    switch (task) {
    case UnknownTask: return "unknown";
    case BeatTask: return "beat";
    case OnsetTask: return "onset";
    case ChordTask: return "chord";
    case SegmentTask: return "segment";
    case KeyTask: return "key";
    case NoteTask: return "note";
    case MelodyTask: return "melody";
    case PitchTask: return "pitch";
    }
    return "unknown";
}

void
JAMSFeatureWriter::finish()
{
    for (FileStreamMap::const_iterator i = m_streams.begin();
	 i != m_streams.end(); ++i) {
	*(i->second) << "}" << endl;
    }

    FileFeatureWriter::finish();
}
