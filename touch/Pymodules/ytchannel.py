//
//	ytchannel.py
//
//	Copyright 2015 Regents of the University of California
//
//	This program is free software : you can redistribute it and / or modify
//	it under the terms of the GNU Lesser General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or
//	(at your option) any later version.
//
//	This program is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
//	GNU Lesser General Public License for more details.
//
//	You should have received a copy of the GNU Lesser General Public License
//	along with this program.If not, see <http://www.gnu.org/licenses/>.
// 
//	Author: Peter Gusev, peter@remap.ucla.edu

from urllib import request
import json
import re
import time
import datetime
import sys
import argparse
import logging

from rfc3339 import rfc3339

try: 
	with open('api.key', 'r') as f:
		apiKey = f.read().strip();
except Exception as err:
	print('exception while reading youtube api key file. make sure it\'s in the same folder as your project')

# Strings
youtubeApiUrl = 'https://www.googleapis.com/youtube/v3/'
youtubeSearchApiUrl = youtubeApiUrl + 'search?key={0}&'.format(apiKey)
requestChannelVideosInfo = youtubeSearchApiUrl + 'channelId={0}&part=id,snippet&fields=items(id,snippet(title))&order=date&type=video&publishedBefore={1}&publishedAfter={2}&pageToken={3}&maxResults=50'
youtubeVideoUrl = 'https://www.youtube.com/watch?v={0}'

# Functions
def getChannelVideosPublishedInInterval(channelId,publishedBefore,publishedAfter):
	retVal = []
	foundAll = False
	
	nextPageToken = ''

	while not foundAll:
		try:
			url = requestChannelVideosInfo.format(channelId,publishedBefore,publishedAfter,nextPageToken)
			print ('request url '+str(url))
			response = request.urlopen(url)
			str_response = response.readall().decode('utf-8')
			responseAsJson = json.loads(str_response)
			response.close()
			returnedVideos = responseAsJson['items']
			
			for video in returnedVideos:
				retVal.append(video) 
				
			try:
				nextPageToken = responseAsJson['nextPageToken']
			except Exception as err:
				foundAll = True
		except Exception as err:
			print('exception while opening URL: '+str(err))
			foundAll = True		
	
	return retVal	
	
def getChannelVideos(channelId,dateToStartFrom,dateToGoBackTo,timeInterval):	
	if(dateToStartFrom < dateToGoBackTo):
		raise Exception('The date to start from cannot be before the date to go back to!')
	
	retVal = []
	
	# initialization
	startFrom = dateToStartFrom
	goBackTo = startFrom - timeInterval
	
	done = False
	
	while not done:
		if(goBackTo < dateToGoBackTo):
			goBackTo = dateToGoBackTo
		
		if(goBackTo == dateToGoBackTo):
			done = True
		
		goBackTo_rfc3339 = rfc3339(goBackTo,utc=True)
		startFrom_rfc3339 = rfc3339(startFrom,utc=True)
		
		videosPublishedInInterval = getChannelVideosPublishedInInterval(channelId,startFrom_rfc3339,goBackTo_rfc3339)
		retVal.extend(videosPublishedInInterval)
		
		if(not done):
			# we simply continue from where we are
			startFrom = goBackTo
			# calculate the next date to go back to based on the given interval
			nextDate = goBackTo - timeInterval
			goBackTo = nextDate
			
	return retVal	


def getVideoURL(videoId):
	retVal = youtubeVideoUrl.format(videoId)
	return retVal

def loadVideosToDat(channelVideos, datName):
	dat = op(datName)
	dat.clear()
	for video in channelVideos:
		videoId = video.get('id').get('videoId')
		videoURL = getVideoURL(videoId)
		videoTitle = video.get('snippet').get('title')
		dat.appendRow([videoURL, videoTitle])

# start point
def loadVideos(channelId):
	try:
		timeInterval = datetime.timedelta(weeks=40)
		t=datetime.date.today()+datetime.timedelta(days=2)
		dateToStartFrom = datetime.datetime(t.year, t.month, t.day)		
		dateToGoBackTo = datetime.datetime.strptime('2015-01-01','%Y-%m-%d')
	
		print('loading channel videos between '+str(dateToStartFrom)+' and '+str(dateToGoBackTo))
		start = time.time()
		channelVideos = getChannelVideos(channelId,dateToStartFrom,dateToGoBackTo,timeInterval)
		end = time.time()
		print(str(len(channelVideos))+ ' videos loaded in '+str(end-start)+' sec')
		
		if(len(channelVideos) > 0):
			for video in channelVideos:
				videoId = video.get('id').get('videoId')
				videoURL = getVideoURL(videoId)
				videoTitle = video.get('snippet').get('title')
				print(videoTitle + ':\t' + videoURL)
				
		loadVideosToDat(channelVideos, 'channelVideos')

	except Exception as err:
		print('exception occurred: '+ str(err))
