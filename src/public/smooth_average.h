#pragma once
// SPDX-License-Identifier: LicenseRef-Valve-Source-1-SDK
// Copyright (c) 1996-2005 Valve Corporation

import std;

import ks.tier1;

// Use this macro around any value, and it'll queue up the results given to it nTimes and 
// provide a running average.
template <typename T1, typename T2>
inline auto SMOOTH_AVERAGE( T1 value, T2 nCount, std::source_location loc = std::source_location::current() )
{
	return CalcSmoothAverage( value, nCount, loc.file_name(), static_cast<int>( loc.line() ) );
}


// Same as their counterpart functions but they return more info in a CTimingInfo structure.
template <typename T1, typename T2>
inline auto SMOOTH_AVERAGE_STRUCT( T1 value, T2 nCount, std::source_location loc = std::source_location::current() )
{
	return CalcSmoothAverage_Struct( value, nCount, loc.file_name(), static_cast<int>( loc.line() ) );
}
template <typename T1, typename T2>
inline auto SUM_OVER_TIME_INTERVAL_STRUCT( T1 value, T2 nSeconds, std::source_location loc = std::source_location::current() )
{
	return SumOverTimeInterval_Struct( value, nSeconds, loc.file_name(), static_cast<int>( loc.line() ) );
}


template< class T >
class CTimingInfo
{
public:
	T	m_AverageValue;	// Note: this will be the SUM of the values if using SUM_OVER_TIME_INTERVAL.
	
	// The high and low points for m_AverageValue over the time interval.
	T	m_HighAverage;	
	T	m_LowAverage;
	
	// The high and low points for the value itself over the time interval.
	T	m_HighValue;
	T	m_LowValue;
};


template< class T >
class CAveragesInfo
{
public:
	class CEntry
	{
	public:
		T	m_Average;
		T	m_Value;
	};

public:	
	std::vector< CEntry > m_Values;
	int m_iCurValue;
};


template< class T >
class CAveragesInfo_TimeBased
{
public:
	class CEntry
	{
	public:
		CCycleCount m_Time;	// When this sample was taken.
		T m_Value;
		T m_Average;
	};
	
	std::vector< CEntry > m_Values;
};



template< class T >
inline T CalcSmoothAverage( const T &value, int nTimes, const char *pFilename, int iLine )
{
	CTimingInfo< T > info = CalcSmoothAverage_Struct( value, nTimes, pFilename, iLine );
	return info.m_AverageValue;
};


template< class T >
inline CTimingInfo< T > SumOverTimeInterval_Struct( const T &value, float nSeconds, const char *pFilename, int iLine )
{
	static ks::idict< CAveragesInfo_TimeBased< T >* > s_SmoothAverages;

	char fullStr[1024];
	V_snprintf( fullStr, sizeof( fullStr ), "%s_%i", pFilename, iLine );
	
	CAveragesInfo_TimeBased<T> *&pInfo = s_SmoothAverages[fullStr];
	if ( !pInfo )
	{
		pInfo = new CAveragesInfo_TimeBased<T>;
	}
	
	// Get the current time now.
	CCycleCount curTime;
	curTime.Sample();
	
	// Get rid of old samples.
	while ( pInfo->m_Values.Count() > 0 && (curTime.GetSeconds() - pInfo->m_Values[0].m_Time.GetSeconds()) > nSeconds )
		pInfo->m_Values.Remove( 0 );

	// Add on the new sample.
	typename CAveragesInfo_TimeBased< T >::CEntry newEntry;
	newEntry.m_Time = curTime;
	newEntry.m_Value = value;
	int newValueIndex = pInfo->m_Values.AddToTail( newEntry );

	CTimingInfo< T > info;
	info.m_AverageValue = pInfo->m_Values[0].m_Value;
	
	info.m_HighAverage = pInfo->m_Values[0].m_Average;
	info.m_LowAverage = pInfo->m_Values[0].m_Average;
	
	info.m_HighValue = pInfo->m_Values[0].m_Value;
	info.m_LowValue = pInfo->m_Values[0].m_Value;

	for ( int i=1; i < pInfo->m_Values.Count(); i++ )
	{
		if ( i != newValueIndex )
		{
			info.m_HighAverage = std::max( pInfo->m_Values[i].m_Average, info.m_HighAverage );
			info.m_LowAverage = std::min( pInfo->m_Values[i].m_Average, info.m_LowAverage );
		}

		info.m_HighValue = std::max( pInfo->m_Values[i].m_Value, info.m_HighValue );
		info.m_LowValue = std::min( pInfo->m_Values[i].m_Value, info.m_LowValue );

		info.m_AverageValue += pInfo->m_Values[i].m_Value;
	}

	pInfo->m_Values[newValueIndex].m_Average = info.m_AverageValue;
	return info;
}


template< class T >
inline CTimingInfo< T > SumOverTimeInterval( const T &value, float nSeconds, const char *pFilename, int iLine )
{
	CTimingInfo< T > info = SumOverTimeInterval_Struct( value, nSeconds, pFilename, iLine );
	return info.m_AverageValue;
}
