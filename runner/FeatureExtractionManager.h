/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Annotator
    A utility for batch feature extraction from audio files.
    Mark Levy, Chris Sutton and Chris Cannam, Queen Mary, University of London.
    Copyright 2007-2008 QMUL.

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef _FEATURE_EXTRACTION_MANAGER_H_
#define _FEATURE_EXTRACTION_MANAGER_H_

#include <vector>
#include <set>
#include <string>
#include <memory>

#include <QMap>

#include <vamp-hostsdk/Plugin.h>
#include <vamp-hostsdk/PluginSummarisingAdapter.h>
#include <transform/Transform.h>

using std::vector;
using std::set;
using std::string;
using std::pair;
using std::map;
using std::shared_ptr;

class FeatureWriter;
class AudioFileReader;

class FeatureExtractionManager
{
public:
    FeatureExtractionManager(bool verbose);
    virtual ~FeatureExtractionManager();

    void setChannels(int channels);
    void setDefaultSampleRate(sv_samplerate_t sampleRate);
    void setNormalise(bool normalise);

    bool setSummaryTypes(const set<string> &summaryTypes,
                         const Vamp::HostExt::PluginSummarisingAdapter::SegmentBoundaries &boundaries);

    void setSummariesOnly(bool summariesOnly);

    bool addFeatureExtractor(Transform transform,
                             const vector<FeatureWriter*> &writers);

    bool addFeatureExtractorFromFile(QString transformXmlFile,
                                     const vector<FeatureWriter*> &writers);

    bool addDefaultFeatureExtractor(TransformId transformId,
                                    const vector<FeatureWriter*> &writers);

    // Make a note of an audio or playlist file which will be passed
    // to extractFeatures later.  Amongst other things, this may
    // initialise the default sample rate and channel count
    void addSource(QString audioSource, bool willMultiplex);

    // Extract features from the given audio or playlist file.  If the
    // file is a playlist and force is true, continue extracting even
    // if a file in the playlist fails.
    void extractFeatures(QString audioSource);

    // Extract features from the given audio files, multiplexing into
    // a single "file" whose individual channels are mixdowns of the
    // supplied sources.
    void extractFeaturesMultiplexed(QStringList sources);

private:
    bool m_verbose;

    // The plugins that we actually "run" may be wrapped versions of
    // the originally loaded ones, depending on the adapter
    // characteristics we need. Because the wrappers use raw pointers,
    // we need to separately retain the originally loaded shared_ptrs
    // so that they don't get auto-deleted. Same goes for any wrappers
    // that may then be re-wrapped. That's what these are for.
    set<shared_ptr<Vamp::PluginBase>> m_allLoadedPlugins;
    set<shared_ptr<Vamp::PluginBase>> m_allAdapters;
    
    // A plugin may have many outputs, so we can have more than one
    // transform requested for a single plugin.  The things we want to
    // run in our process loop are plugins rather than their outputs,
    // so we maintain a map from the plugins to the transforms desired
    // of them and then iterate through this map
    typedef map<Transform, vector<FeatureWriter *> > TransformWriterMap;
    typedef map<shared_ptr<Vamp::Plugin>, TransformWriterMap> PluginMap;
    PluginMap m_plugins;

    // When we run plugins, we want to run them in a known order so as
    // to get the same results on each run of Sonic Annotator with the
    // same transforms. But if we just iterate through our PluginMap,
    // we get them in an arbitrary order based on pointer
    // address. This vector provides an underlying order for us. Note
    // that the TransformWriterMap is consistently ordered (because
    // the key is a Transform which has a proper ordering) so using
    // this gives us a consistent order across the whole PluginMap
    vector<shared_ptr<Vamp::Plugin>> m_orderedPlugins;

    // And a map back from transforms to their plugins.  Note that
    // this is keyed by whole transform structure, not transform ID --
    // two differently configured transforms with the same ID must use
    // different plugin instances.
    typedef map<Transform, shared_ptr<Vamp::Plugin>> TransformPluginMap;
    TransformPluginMap m_transformPluginMap;

    // Cache the plugin output descriptors, mapping from plugin to a
    // map from output ID to output descriptor.
    typedef map<string, Vamp::Plugin::OutputDescriptor> OutputMap;
    typedef map<shared_ptr<Vamp::Plugin>, OutputMap> PluginOutputMap;
    PluginOutputMap m_pluginOutputs;

    // Map from plugin output identifier to plugin output index
    typedef map<string, int> OutputIndexMap;
    OutputIndexMap m_pluginOutputIndices;

    typedef set<std::string> SummaryNameSet;
    SummaryNameSet m_summaries; // requested on command line for all transforms
    bool m_summariesOnly; // command line flag
    Vamp::HostExt::PluginSummarisingAdapter::SegmentBoundaries m_boundaries;

    AudioFileReader *prepareReader(QString audioSource);

    void extractFeaturesFor(AudioFileReader *reader, QString audioSource);

    void writeSummaries(QString audioSource,
                        std::shared_ptr<Vamp::Plugin>);

    void writeFeatures(QString audioSource,
                       std::shared_ptr<Vamp::Plugin>,
                       const Vamp::Plugin::FeatureSet &,
                       Transform::SummaryType summaryType =
                       Transform::NoSummary);

    void testOutputFiles(QString audioSource);
    void finish();

    int m_blockSize;
    sv_samplerate_t m_defaultSampleRate;
    sv_samplerate_t m_sampleRate;
    int m_channels;
    bool m_normalise;

    QMap<QString, AudioFileReader *> m_readyReaders;
};

#endif
