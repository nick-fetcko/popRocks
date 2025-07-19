/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
  Vamp feature extraction plugin for the BeatRoot beat tracker.

  Centre for Digital Music, Queen Mary, University of London.
  This file copyright 2011 Simon Dixon, Chris Cannam and QMUL.
    
  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License as
  published by the Free Software Foundation; either version 2 of the
  License, or (at your option) any later version.  See the file
  COPYING included with this distribution for more information.
*/

#include "Peaks.h"

int Peaks::pre = 3;
int Peaks::post = 1;

// Modified 18Jul2025 by Nick Fetcko:
//    Changed from int to std::size_t
//    to prevent C4267
std::size_t Peaks::findPeaks(const vector<double> &data, vector<std::size_t> peaks, int width) {
    std::size_t peakCount = 0;
    std::size_t maxp = 0;
    std::size_t mid = 0;
    std::size_t end = data.size();
    while (mid < end) {
        std::size_t i = mid - width;
        if (i < 0)
            i = 0;
        std::size_t stop = mid + width + 1;
        if (stop > data.size())
            stop = data.size();
        maxp = i;
        for (i++; i < stop; i++)
            if (data[i] > data[maxp])
                maxp = i;
        if (maxp == mid) {
            std::size_t j;
            for (j = peakCount; j > 0; j--) {
                if (data[maxp] <= data[peaks[j-1]])
                    break;
                else if (j < peaks.size())
                    peaks[j] = peaks[j-1];
            }
            if (j != peaks.size())
                peaks[j] = maxp;
            if (peakCount != peaks.size())
                peakCount++;
        }
        mid++;
    }
    return peakCount;
} // findPeaks()

vector<std::size_t> Peaks::findPeaks(const vector<double> &data, int width,
                             double threshold, double decayRate, bool isRelative) {
    vector<std::size_t> peaks;

	// Modified 18Jul2025 by Nick Fetcko:
	//    Changed from int to std::size_t
	//    to prevent C4267
    std::size_t maxp = 0;
    std::size_t mid = 0;
    if (data.empty()) return peaks;
    std::size_t end = data.size();
    double av = data[0];
    while (mid < end) {
        av = decayRate * av + (1 - decayRate) * data[mid];
        if (av < data[mid])
            av = data[mid];
        std::size_t i = 
			width > mid ? 0 : mid - width;
        std::size_t stop = mid + width + 1;
        if (stop > data.size())
            stop = data.size();
        maxp = i;
        for (i++; i < stop; i++)
            if (data[i] > data[maxp])
                maxp = i;
        if (maxp == mid) {
            if (overThreshold(data, maxp, width, threshold, isRelative,av)){
                peaks.push_back(maxp);
            }
        }
        mid++;
    }
    return peaks;
} // findPeaks()

double Peaks::expDecayWithHold(double av, double decayRate,
                               const vector<double> &data, int start, int stop) {
    while (start < stop) {
        av = decayRate * av + (1 - decayRate) * data[start];
        if (av < data[start])
            av = data[start];
        start++;
    }
    return av;
} // expDecayWithHold()

// Modified 18Jul2025 by Nick Fetcko:
//    Changed from int to std::size_t
//    to prevent C4267
bool Peaks::overThreshold(const vector<double> &data, std::size_t index, int width,
                          double threshold, bool isRelative,
                          double av) {
    if (data[index] < av)
        return false;
    if (isRelative) {
        std::size_t iStart = 
			pre * width > index ?
			0 :
			index - pre * width;
        std::size_t iStop = index + post * width;
        if (iStop > data.size())
            iStop = data.size();
        double sum = 0;
        std::size_t count = iStop - iStart;
        while (iStart < iStop)
            sum += data[iStart++];
        return (data[index] > sum / count + threshold);
    } else
        return (data[index] > threshold);
} // overThreshold()

void Peaks::normalise(vector<double> &data) {
    double sx = 0;
    double sxx = 0;
    for (int i = 0; i < (int)data.size(); i++) {
		if (data[i] == std::numeric_limits<double>::infinity() ||
			data[i] == std::numeric_limits<double>::quiet_NaN() ||
			data[i] == std::numeric_limits<double>::signaling_NaN())
			continue;
        sx += data[i];
        sxx += data[i] * data[i];
    }
    double mean = sx / data.size();
    double sd = sqrt((sxx - sx * mean) / data.size());
    if (sd == 0)
        sd = 1;		// all data[i] == mean  -> 0; avoids div by 0
    for (int i = 0; i < (int)data.size(); i++) {
        data[i] = (data[i] - mean) / sd;
    }
} // normalise()

/** Uses an n-point linear regression to estimate the slope of data.
 *  @param data input data
 *  @param hop spacing of data points
 *  @param n length of linear regression
 *  @param slope output data
 */
void Peaks::getSlope(const vector<double> &data, double hop, int n,
                     vector<double> &slope) {
    int i = 0, j = 0;
    double t;
    double sx = 0, sxx = 0, sy = 0, sxy = 0;
    for ( ; i < n; i++) {
        t = i * hop;
        sx += t;
        sxx += t * t;
        sy += data[i];
        sxy += t * data[i];
    }
    double delta = n * sxx - sx * sx;
    for ( ; j < n / 2; j++)
        slope[j] = (n * sxy - sx * sy) / delta;
    for ( ; j < (int)data.size() - (n + 1) / 2; j++, i++) {
        slope[j] = (n * sxy - sx * sy) / delta;
        sy += data[i] - data[i - n];
        sxy += hop * (n * data[i] - sy);
    }
    for ( ; j < (int)data.size(); j++)
        slope[j] = (n * sxy - sx * sy) / delta;
} // getSlope()

int Peaks::imin(const vector<double> &arr) {
    int i = 0;
    for (int j = 1; j < (int)arr.size(); j++)
        if (arr[j] < arr[i])
            i = j;
    return i;
} // imin()

int Peaks::imax(const vector<double> &arr) {
    int i = 0;
    for (int j = 1; j < (int)arr.size(); j++)
        if (arr[j] > arr[i])
            i = j;
    return i;
} // imax()

