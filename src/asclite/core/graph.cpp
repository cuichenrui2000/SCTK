/*
 * asclite
 * Author: Jerome Ajot, Nicolas Radde, Chris Laprun
 *
 * This software was developed at the National Institute of Standards and Technology by
 * employees of the Federal Government in the course of their official duties.  Pursuant to
 * Title 17 Section 105 of the United States Code this software is not subject to copyright
 * protection within the United States and is in the public domain. asclite is
 * an experimental system.  NIST assumes no responsibility whatsoever for its use by any party.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS."  With regard to this software, NIST MAKES NO EXPRESS
 * OR IMPLIED WARRANTY AS TO ANY MATTER WHATSOEVER, INCLUDING MERCHANTABILITY,
 * OR FITNESS FOR A PARTICULAR PURPOSE.
 */
 
/**
 * Inplementation of the Levenshtein Distance Algorithm in Multi-Dimension
 */
	
#include "graph.h"

Logger* Graph::logger = Logger::getLogger();

/** Constructor with the list of segments and the position of the first ref */
Graph::Graph(SegmentsGroup* _segments, SpeakerMatch* _pSpeakerMatch, int _costTrans, int _costIns, int _costOpt, int _costCorrectNonSpeaker, int _costAdaptive, bool _optRef, bool _optHyp, bool _bCompressedArray) : m_CostTransition(_costTrans), m_CostInsertion(_costIns), m_CostOptionally(_costOpt), m_CostCorrectNonSpeaker(_costCorrectNonSpeaker), m_CostAdaptive(_costAdaptive), m_useOptForRef(_optRef), m_useOptForHyp(_optHyp)
{
	m_bCompressedArray = _bCompressedArray;
	m_pSpeakerMatch = _pSpeakerMatch;
	Token* curToken;
	size_t i, k, sizevector;
	
	SetDimension(_segments->GetNumberOfReferences()+_segments->GetNumberOfHypothesis());
	SetIndexRef(_segments->GetNumberOfHypothesis());
	
	if(_segments->GetNumberOfHypothesis() == 0)
		m_HypRefStatus = 0;
	else
		m_HypRefStatus = 1;
	
	m_TabDimensionDeep = new size_t[GetDimension()];
	m_TabVecHypRef = new vector<Token*>[GetDimension()];
	m_TabMapTokenIndex = new map<Token*, size_t>[GetDimension()];
	m_TabFirstTokens = new list<Token*>[GetDimension()];
	m_TabLastTokens = new list<Token*>[GetDimension()];
	    
	// Planning each Segment and look for the last and first token
	for(i=0; i<GetDimension(); ++i)
	{
        vector<Segment*> temp_segs;
		
        if (i < _segments->GetNumberOfHypothesis())
        {
            m_TabVecHypRef[i] = _segments->ToTopologicalOrderedStructHyp(i);
            temp_segs = _segments->GetHypothesis(i);
        }
        else
        {
            m_TabVecHypRef[i] = _segments->ToTopologicalOrderedStructRef(i-_segments->GetNumberOfHypothesis());
            temp_segs = _segments->GetReference(i-_segments->GetNumberOfHypothesis());
        }
		
		sizevector = m_TabVecHypRef[i].size();
		        				
		SetDimensionDeep(i, sizevector);
		
		for(k=0; k<sizevector; ++k)
		{
			curToken = m_TabVecHypRef[i][k];
			
			if(curToken != NULL)
			{
				//cerr << i << " " << k << " " << curToken->ToString() << endl;
				
				m_TabMapTokenIndex[i][curToken] = k;
				
				//cerr << " " << i << " " << k << " " << curToken->ToString() << endl;
        
				size_t prcs = 0;
				
				while( (prcs < (size_t)(temp_segs.size()-1)) && (temp_segs[prcs]->isEmpty()) )
				{
					++prcs;
				}
				
				if(temp_segs[prcs]->isFirstToken(curToken))
				{
                    m_TabFirstTokens[i].push_front(curToken);
					//cerr << "Last Token: " << curToken->ToString() << endl;
				}
				
				prcs = temp_segs.size()-1;
				
				while( (prcs > 0) && (temp_segs[prcs]->isEmpty()) )
				{
					--prcs;
				}
				
				//if(temp_segs[temp_segs.size()-1]->isLastToken(curToken))
				if(temp_segs[prcs]->isLastToken(curToken))
				{
                    m_TabLastTokens[i].push_front(curToken);
					//cerr << "First Token: " << curToken->ToString() << endl;
				}
			}
		}
	}
	
	SetGraphOptimization();
	m_MaxDurationSegmentGroup = _segments->GetTotalDuration();
	
	if(m_bCompressedArray)
	{
		LOG_DEBUG(logger, "Lenvenshtein Matrix Compression: ON");
		m_MapCost = new CompressedLevenshteinMatrix(GetDimension(), m_TabDimensionDeep);
	}
	else
	{
		LOG_DEBUG(logger, "Lenvenshtein Matrix Compression: OFF");
		m_MapCost = new ArrayLevenshteinMatrix(GetDimension(), m_TabDimensionDeep);
	}
	    
    // Creating cache container
    m_TabCacheDimPreviousIndex = new list<size_t>** [GetDimension()];
    
    for(i=0; i<GetDimension(); ++i)
    {
        m_TabCacheDimPreviousIndex[i] = new list<size_t>* [m_TabDimensionDeep[i]];
        
        for(k=0; k<m_TabDimensionDeep[i]; ++k)
            m_TabCacheDimPreviousIndex[i][k] = NULL;
    }
}

/** Destructor */
Graph::~Graph()
{
    // Cleaning cache
    for(size_t i=0; i<GetDimension(); ++i)
	{
		for(size_t j=0; j<m_TabDimensionDeep[i]; ++j)
        {
            if(m_TabCacheDimPreviousIndex[i][j])
                m_TabCacheDimPreviousIndex[i][j]->clear();
        }
        
        delete [] m_TabCacheDimPreviousIndex[i];
	}
    
    delete [] m_TabCacheDimPreviousIndex;
    
	delete m_MapCost;
	
	for(size_t i=0; i<GetDimension(); ++i)
	{
		m_TabVecHypRef[i].clear();
		m_TabFirstTokens[i].clear();
		m_TabLastTokens[i].clear();		
		m_TabMapTokenIndex[i].clear();
	}
	
	delete [] m_TabDimensionDeep;
	delete [] m_TabVecHypRef;
	delete [] m_TabFirstTokens;
	delete [] m_TabLastTokens;
	delete [] m_TabMapTokenIndex;   
}

/** Set the dimension */
void Graph::SetDimension(size_t dim)
{
	if(dim > 1)
		m_Dimension = dim;
	else
	{
        char buffer [BUFFER_SIZE];
        sprintf(buffer, "Graph::SetDimension()\nInvalid dimension (%li)!", dim);
		LOG_FATAL(logger, buffer);
		exit(0);
	}
}

/** Set the deep of one dimension */
void Graph::SetDimensionDeep(size_t dim, size_t deep)
{
	if(dim < m_Dimension)
		m_TabDimensionDeep[dim] = deep;
	else
	{
        char buffer [BUFFER_SIZE];
        sprintf(buffer, "Graph::SetDimensionDeep()\nInvalid dimension (%li), max: %li\nExiting!", dim, m_Dimension);
		LOG_FATAL(logger, buffer);
		exit(0);
	}
}

/** Set the position of the first ref */
void Graph::SetIndexRef(size_t ind)
{
    if(ind < m_Dimension)
		m_IndexRef = ind;
	else
	{
        char buffer [BUFFER_SIZE];
        sprintf(buffer, "Graph::SetIndexRef()\nInvalid dimension (%li), max: %li\nExiting!", ind, m_Dimension);
		LOG_FATAL(logger, buffer);
		exit(0);
	}
}

/** Calculate the cost for the coordinate */
int Graph::CalculateCost(size_t* curcoord)
{
	GraphCoordinateList listprevs(GetDimension());
	PreviousCoordinates(listprevs, curcoord);
	
	if(listprevs.isEmpty())
	{
		// It's a first one
		listprevs.RemoveAll();
		return 0;
	}
	
	GraphCoordinateListPosition i, ei;
	int mincost = C_UNCALCULATED, cost;
	size_t* coordinate = NULL;
	
	i = listprevs.GetBeginPosition();
	ei = listprevs.GetEndPosition();
	
	while(i != ei)
	{
		if(coordinate)
			delete [] coordinate;
		
		coordinate = listprevs.GetAt(i);
		cost = m_MapCost->GetCostFor(coordinate);
		
		if(cost == C_UNCALCULATED)
		{
            cost = CalculateCost(coordinate);
            m_MapCost->SetCostFor(coordinate, cost);
		}
		
		if(mincost == C_UNCALCULATED)
		{
			mincost = cost + GetTransitionCost(curcoord, coordinate);
		}
		else
		{
			int CostTransCost = cost + GetTransitionCost(curcoord, coordinate);
			
			if(CostTransCost < mincost)
			{
				mincost = CostTransCost;
			}
		}

		listprevs.NextPosition(i);
	}
	
	// Start cleaning memory	
	listprevs.RemoveAll();
	
	if(coordinate)
		delete [] coordinate;
	// End cleaning memory
	
	return mincost;
}

/** Create the list of starting coordinates */
void Graph::StartingCoordinates(GraphCoordinateList& listStart)
{
	list<Token*>::iterator* tabInteratorListBegin = new list<Token*>::iterator[GetDimension()];
	list<Token*>::iterator* tabInteratorListEnd = new list<Token*>::iterator[GetDimension()];
	list<Token*>::iterator* tabInteratorListCurrent = new list<Token*>::iterator[GetDimension()];
	size_t i, curdim;
	bool inccurdim;
	size_t* startcoord = new size_t[GetDimension()];
	
	listStart.RemoveAll();
	
	for(i=0; i<GetDimension(); ++i)
	{
		if(!isHypRefEmpty(i))
		{
			tabInteratorListBegin[i] = m_TabLastTokens[i].begin();
			tabInteratorListCurrent[i] = m_TabLastTokens[i].begin();
			tabInteratorListEnd[i] = m_TabLastTokens[i].end();
		}
	}
	
	do
	{
		for(i=0; i<GetDimension(); ++i)
		{
			if(!isHypRefEmpty(i))
				startcoord[i] = m_TabMapTokenIndex[i][*(tabInteratorListCurrent[i])];
			else
				startcoord[i] = 0;
		}

		listStart.AddFront(startcoord);
	
		curdim = 0;
		
		do
		{
			if(!isHypRefEmpty(curdim))
			{
				++(tabInteratorListCurrent[curdim]);
				inccurdim = (tabInteratorListCurrent[curdim] == tabInteratorListEnd[curdim]);
			}
			else
			{
				inccurdim = true;
			}
			
			if(inccurdim)
			{
				tabInteratorListCurrent[curdim] = tabInteratorListBegin[curdim];
				++curdim;
				
				if(curdim == GetDimension())
					inccurdim = false;
			}
		}
		while(inccurdim);
	}
	while( curdim != GetDimension() );
		
	// Start cleaning memory
	delete [] tabInteratorListBegin;
	delete [] tabInteratorListEnd;
	delete [] tabInteratorListCurrent;
	delete [] startcoord;
	// End cleaning memory
}

/** is the one of the last possible coordinates ? */
bool Graph::isEndingCoordinate(size_t* coord)
{
	for(size_t k=0; k<GetDimension(); ++k)
	{
		if(coord[k] != 0)
			return false;
	}
	
	return true;
}

/** Fill the graph with cost */
void Graph::FillGraph()
{
	GraphCoordinateList listStartingCoordinates(GetDimension());
	GraphCoordinateListPosition i, ei;
	size_t* coordinate = NULL;
		
	StartingCoordinates(listStartingCoordinates);
	
	i = listStartingCoordinates.GetBeginPosition();
	ei = listStartingCoordinates.GetEndPosition();
	
	while(i != ei)
	{
		if(coordinate)
			delete [] coordinate;
		
		coordinate = listStartingCoordinates.GetAt(i);
				
		if(!m_MapCost->IsCostCalculatedFor(coordinate))		
		{
			m_MapCost->SetCostFor(coordinate, CalculateCost(coordinate));
		}
		
		listStartingCoordinates.NextPosition(i);
	}
	
	// Start cleaning memory
	if(coordinate)
		delete [] coordinate;
	
	listStartingCoordinates.RemoveAll();
	// End cleaning memory
}

/** returns the list of previous indexes */
void Graph::PreviousIndexes(list<size_t>& listPrev, size_t dim, size_t index)
{
	listPrev.clear();
    
	// Asking for the previous tokens of the last
	if(index == 0)
	{
		listPrev.push_front(0);
		return;
	}

    list<size_t>* listprevious = m_TabCacheDimPreviousIndex[dim][index];
    
    if(listprevious)
    {
        listPrev = *listprevious;
        return;
    }
        
    m_TabCacheDimPreviousIndex[dim][index] = new list<size_t>;
	
	list<Token*>::iterator i, ei;
	bool is0added = false;
	
	// Asking for the first tokens to work on
	if(index == GetDimensionDeep(dim)-1)
	{
		i = m_TabLastTokens[dim].begin();
		ei = m_TabLastTokens[dim].end();
		
		while(i != ei)
		{
			if( (*i == NULL) && (!is0added) )
			{
				is0added = true;
				//listPrev.push_front(0);
                m_TabCacheDimPreviousIndex[dim][index]->push_front(0);
			}
			else
			{
				//listPrev.push_front(m_TabMapTokenIndex[dim][*i]);
                m_TabCacheDimPreviousIndex[dim][index]->push_front(m_TabMapTokenIndex[dim][*i]);
			}

			++i;
		}
	}
	else
	{
		i = m_TabFirstTokens[dim].begin();
		ei = m_TabFirstTokens[dim].end();
		
		while(i != ei)
		{
			if( (*i == m_TabVecHypRef[dim][index]) && (!is0added) )
			{
				is0added = true;
				//listPrev.push_front(0);
                m_TabCacheDimPreviousIndex[dim][index]->push_front(0);
			}
			else
			{
				Token* tokenIndex = m_TabVecHypRef[dim][index];
				size_t j, nbprevtokens = tokenIndex->GetNbOfPrecTokens();
				
				if(nbprevtokens == 0)
				{
					//listPrev.push_front(0);
                    m_TabCacheDimPreviousIndex[dim][index]->push_front(0);
				}
				else
				{
					for(j=0; j<nbprevtokens; ++j)
					{
						//listPrev.push_front(m_TabMapTokenIndex[dim][tokenIndex->GetPrecToken(j)]);
                        m_TabCacheDimPreviousIndex[dim][index]->push_front(m_TabMapTokenIndex[dim][tokenIndex->GetPrecToken(j)]);
					}
				}
			}
			
			++i;
		}
	}
    
    listPrev = *(m_TabCacheDimPreviousIndex[dim][index]);
}

/** List the previous coordinates */
void Graph::PreviousCoordinatesHypRef(GraphCoordinateList& listPrev, size_t* coord)
{
	listPrev.RemoveAll();
	
	if(isEndingCoordinate(coord))
		return;
	
	list<size_t>* tabPreviousIndexes = new list<size_t>[GetDimension()];
    list<size_t>::iterator l, le, m, me;
	size_t* prevcoordr = new size_t[GetDimension()];
	size_t* prevcoordh = new size_t[GetDimension()];
		
	for(size_t i=0; i<GetDimension(); ++i)
		PreviousIndexes(tabPreviousIndexes[i], i, coord[i]);
		
	// Change only one coordinate into the Refs and both
	for(size_t i=m_IndexRef; i<GetDimension(); ++i)
	{
		if(coord[i] != 0)
		{
			l = tabPreviousIndexes[i].begin();
			le = tabPreviousIndexes[i].end();
			
			while(l != le)
			{
				for(size_t j=0; j<GetDimension(); ++j)
					prevcoordr[j] = coord[j];
				
				prevcoordr[i] = *l;
				
				if(ValidateTransitionInsertionDeletion(coord, prevcoordr))
					listPrev.AddFront(prevcoordr);
				
				for(size_t j=0; j<m_IndexRef; ++j)
				{
					if(coord[j] != 0)
					{
						m = tabPreviousIndexes[j].begin();
						me = tabPreviousIndexes[j].end();
						
						while(m != me)
						{
							for(size_t k=0; k<GetDimension(); ++k)
								prevcoordh[k] = prevcoordr[k];
							
							prevcoordh[j] = *m;
							
							if(ValidateTransitionInsertionDeletion(coord, prevcoordh))
								listPrev.AddFront(prevcoordh);
							
							++m;
						}
					}
				}
				
				++l;
			}
		}
	}
	
	// Change only one coordinate into the Hyps
	for(size_t i=0; i<m_IndexRef; ++i)
	{
		if(coord[i] != 0)
		{
			l = tabPreviousIndexes[i].begin();
			le = tabPreviousIndexes[i].end();
			
			while(l != le)
			{
				for(size_t j=0; j<GetDimension(); ++j)
					prevcoordr[j] = coord[j];
				
				prevcoordr[i] = *l;
				
				if(ValidateTransitionInsertionDeletion(coord, prevcoordr))
					listPrev.AddFront(prevcoordr);
				
				++l;
			}
		}
	}

	// Start cleaning memory
	for(size_t i=0; i<GetDimension(); ++i)
		tabPreviousIndexes[i].clear();
	
	delete [] prevcoordr;
	delete [] prevcoordh;
	delete [] tabPreviousIndexes;
	// End cleaning memory
}

/** List the previous coordinates generic way to compute */
void Graph::PreviousCoordinatesGeneric(GraphCoordinateList& listPrev, size_t* coord)
{
	listPrev.RemoveAll();
	
	if(isEndingCoordinate(coord))
		return;
	
	size_t i, j;
	list<size_t>* tabPreviousIndexes = new list<size_t>[GetDimension()];
	list<size_t>::iterator* tabInteratorListBegin = new list<size_t>::iterator[GetDimension()];
	list<size_t>::iterator* tabInteratorListEnd = new list<size_t>::iterator[GetDimension()];
	list<size_t>::iterator* tabInteratorListCurrent = new list<size_t>::iterator[GetDimension()];
	
	for(i=0; i<GetDimension(); ++i)
		PreviousIndexes(tabPreviousIndexes[i], i, coord[i]);
	
	for(i=0; i<GetDimension(); ++i)
	{
		tabInteratorListBegin[i] = tabPreviousIndexes[i].begin();
		tabInteratorListEnd[i] = tabPreviousIndexes[i].end();
		
		if(coord[i] != 0)
			tabInteratorListCurrent[i] = tabPreviousIndexes[i].begin();
		else
			tabInteratorListCurrent[i] = tabPreviousIndexes[i].end();
	}
	
	while(tabInteratorListCurrent[GetDimension()-1] != tabInteratorListEnd[GetDimension()-1])
	{
		size_t* startcoord = new size_t[GetDimension()];
		
		for(i=0; i<GetDimension(); ++i)
			startcoord[i] = *(tabInteratorListCurrent[i]);
		
		listPrev.AddFront(startcoord);
		
		j = 0;
		
		while(j != GetDimension())
		{
			++(tabInteratorListCurrent[j]);
			
			if( (tabInteratorListCurrent[j] == tabInteratorListEnd[j]) && (j<GetDimension()-1) )
			{
				tabInteratorListCurrent[j] == tabInteratorListBegin[j];
				++j;
			}
			else
				j = GetDimension();
		}
	}
	
	for(i=0; i<GetDimension(); ++i)
		tabPreviousIndexes[i].clear();
	
	delete [] tabPreviousIndexes;
	delete [] tabInteratorListBegin;
	delete [] tabInteratorListEnd;
	delete [] tabInteratorListCurrent;
}

/** returns the cost between 2 coordinates for Hyp-Ref constraints */
int Graph::GetTransitionCostHypRef(size_t* coordcurr, size_t* coordprev)
{
	size_t tok1Index = 0;
	size_t i = 0;
	Token* pToken1 = NULL;
	Token* pToken2 = NULL;
	bool bT1 = false;
	bool bT2 = false;
	bool deletable = false;
	
	while( (i != GetDimension()) && !bT2)
	{
		if(coordcurr[i] != coordprev[i])
		{
			if(!bT1)
			{
				pToken1 = m_TabVecHypRef[i][coordcurr[i]];
				tok1Index = i;
				bT1 = true;
			}
			else
			{
				pToken2 = m_TabVecHypRef[i][coordcurr[i]];
				bT2 = true;
			}
		}
		
		++i;
	};
	
	if(!bT2)
	// Insertion or Deletion
	{
    //Set the token as optionnaly only if it's activated for the specific ref-hyp case
		if(pToken1 && ((tok1Index < m_IndexRef && m_useOptForHyp) || (tok1Index >= m_IndexRef && m_useOptForRef)))
		{
            //if (pToken1->IsOptional())
            //  LOG_DEBUG(logger, "got an optionnaly");
		  deletable = pToken1->IsOptional();
        }
		
		//return( GetCostInsertion(deletable)*( GetDimension()-1 ) ); // MD without restriction
		return( GetCostInsertion(deletable) ); // MD with restriction
	}
	else
	{
		// MD with restriction
		if( ( (!pToken1) && (pToken2) ) || ( (pToken1) && (!pToken2) ) )
			return GetCostTransition();
		else if( (!pToken1) && (!pToken2) )
			return 0;
		else
			//return(GetCostTransition()*( !(pToken1->IsEquivalentTo(pToken2)) ));
			return(GetCostTransition(pToken1, pToken2));
		
	}
}

/** returns cost of transition */
int Graph::GetCostTransition(Token* pToken1, Token* pToken2)
{
	int AdaptiveCost = GetCostAdaptive(pToken1, pToken2);
	
	if(! pToken1->IsEquivalentTo(pToken2) )
	{
		return m_CostTransition + AdaptiveCost;
	}
	else
	{
		if(!m_bSpeakerOptimization)
		{
			return 0 + AdaptiveCost;
		}
		else
		{
			string file1 = pToken1->GetParentSegment()->GetSource();
			string channel1 = pToken1->GetParentSegment()->GetChannel();
			string speaker1 = pToken1->GetParentSegment()->GetSpeakerId();
			transform(speaker1.begin(), speaker1.end(), speaker1.begin(), (int(*)(int)) toupper);
			
			string file2 = pToken2->GetParentSegment()->GetSource();
			string channel2 = pToken2->GetParentSegment()->GetChannel();
			string speaker2 = pToken2->GetParentSegment()->GetSpeakerId();
			transform(speaker2.begin(), speaker2.end(), speaker2.begin(), (int(*)(int)) toupper);
			
			if( (file1 != file2) || (channel1 != channel2) )
			{
				LOG_FATAL(logger, "Error file and channel mismatch " + file1 + " " + channel1 + " " + file2 + " " + channel2); 
				exit(0);
			}
						
			if(m_pSpeakerMatch->GetRef(file1, channel1, speaker1) == speaker2)
			{
				return 0 + AdaptiveCost;
			}
			else
			{
				return m_CostCorrectNonSpeaker + AdaptiveCost;
			}
		}
	}
}

/** returns the cost between 2 coordinates generic way to compute */
int Graph::GetTransitionCostGeneric(size_t* coordcurr, size_t* coordprev)
{
	size_t nbrchange = 0;
	size_t nbrdiff = 0;
	size_t nbroccur = 0;
	list<Token*> listToken;
	list<Token*>::iterator j, ej;
	bool found, deletable;
	
	deletable = false;
	
	for(size_t i=0; i<GetDimension(); ++i)
	{
		if(coordcurr[i] != coordprev[i])
		{
			Token* aToken = m_TabVecHypRef[i][coordcurr[i]];
			
			j = listToken.begin();
			ej = listToken.end();
			found = false;
			
			while( (j != ej) && (!found) )
			{
				if(aToken)
					found = aToken->IsEquivalentTo(*j);
				
				++j;
			}
			
			if(!found)
			{
				++nbrdiff;
				listToken.push_front(aToken);
			}
			
			++nbrchange;
		}
	}
	
	if(nbrchange == 1)
		deletable = (*(listToken.begin()))->IsOptional();
	
	listToken.clear();
	
	if(nbrdiff == GetDimension())
	{
		nbroccur = 1;
	}
	else
	{
		nbroccur = GetDimension() - nbrdiff + 1;
	}
	
	return( GetCostInsertion(deletable)*( GetDimension() - nbrchange ) + GetCostTransition()*( nbrchange - nbroccur ) );
}

/** Return the best (min) cost at the end of the graph */
int Graph::GetBestCost()
{
	int bestcost = -1;
	GraphCoordinateList listStartingCoordinates(GetDimension());
	int currcost;
	
	StartingCoordinates(listStartingCoordinates);
	
	GraphCoordinateListPosition i = listStartingCoordinates.GetBeginPosition();
	GraphCoordinateListPosition ei = listStartingCoordinates.GetEndPosition();
	
	while(i != ei)
	{
		size_t* coordinate = listStartingCoordinates.GetAt(i);
		currcost = m_MapCost->GetCostFor(coordinate);
		
		if(bestcost == -1)
			bestcost = currcost;
		else
			if(currcost < bestcost)
				bestcost = currcost;
		
		listStartingCoordinates.NextPosition(i);
			
		if(coordinate)
			delete [] coordinate;
	}
	
	// Start cleaning memory
	listStartingCoordinates.RemoveAll();
	// End cleaning memory
	
	return bestcost;
}

/** Returns the alignment */
GraphAlignedSegment* Graph::RetrieveAlignment()
{	
	GraphAlignedSegment* outAlign = new GraphAlignedSegment(m_IndexRef);
	size_t* curCoordinate = NULL;
	size_t* bestprevCoordinate = NULL;
	size_t* coordinate = NULL;
	
	int prevbestcost = -1, currcost;
	GraphCoordinateList listPrevCoordinates(GetDimension());
	size_t it;
	
	StartingCoordinates(listPrevCoordinates);
		
	GraphCoordinateListPosition i = listPrevCoordinates.GetBeginPosition();
	GraphCoordinateListPosition ei = listPrevCoordinates.GetEndPosition();
	
	while(i != ei)
	{
		if(coordinate && (bestprevCoordinate != coordinate) && (curCoordinate != coordinate))
			delete [] coordinate;
		
		coordinate = listPrevCoordinates.GetAt(i);
		currcost = m_MapCost->GetCostFor(coordinate);
		
		if(prevbestcost == -1)
		{
			prevbestcost = currcost;
			curCoordinate = coordinate;
		}
		else if(currcost < prevbestcost)
		{
            prevbestcost = currcost;
            curCoordinate = coordinate;
		}
			
		listPrevCoordinates.NextPosition(i);
	}
	
	while((bestprevCoordinate = GetBestCoordinateAndCost(curCoordinate)) != NULL)
	{
		GraphAlignedToken* aGraphAlignedToken = new GraphAlignedToken(GetDimension());
		Token* token;
		
		for(it=0; it<GetDimension(); ++it)
		{
			token = NULL;
			
			if(curCoordinate[it] != bestprevCoordinate[it])
			{
				token = m_TabVecHypRef[it][curCoordinate[it]];
			}

			aGraphAlignedToken->SetToken(it, token);
		}
		
		outAlign->AddFrontGraphAlignedToken(aGraphAlignedToken);
		
		//cerr << aGraphAlignedToken->ToString();
		
		for(it=0; it<GetDimension(); ++it)
		{
			curCoordinate[it] = bestprevCoordinate[it];
		}
		
		delete [] bestprevCoordinate;
	}
	
	// Start cleaning memory
	listPrevCoordinates.RemoveAll();
	
	if(coordinate)
		delete [] coordinate;
	
	if(curCoordinate && (curCoordinate != bestprevCoordinate) && (curCoordinate != coordinate))
		delete [] curCoordinate;
	// End cleaning memory
	
	return outAlign;
}

/** Returns the best previous coordinate */
size_t* Graph::GetBestCoordinateAndCost(size_t* coordcurr)
{
	if(isEndingCoordinate(coordcurr))
		return NULL;
	
	GraphCoordinateList listPrevCoordinates(GetDimension());
	size_t* bestprev = NULL;
	int curcost;
	
	PreviousCoordinates(listPrevCoordinates, coordcurr);
	GraphCoordinateListPosition i = listPrevCoordinates.GetBeginPosition();
	GraphCoordinateListPosition ei = listPrevCoordinates.GetEndPosition();
	int prevbestcost = -1;
	size_t bestnumchg = 0;
	bool isCorrectSubs = false;
	
	while(i != ei)
	{
		size_t* coordinate = listPrevCoordinates.GetAt(i);
		curcost = m_MapCost->GetCostFor(coordinate) + GetTransitionCost(coordcurr, coordinate);

		if(prevbestcost == -1)
		{
			prevbestcost = curcost;
			
			if(bestprev)
				delete [] bestprev;
			
			bestprev = coordinate;
			
			bool bToken1 = false;
			bool bToken2 = false;
			size_t c=0;
			
			while( (c != GetDimension()) && !bToken2)
			{
				if(coordcurr[c] != coordinate[c])
				{
					if(!bToken1)
						bToken1 = true;
					else
						bToken2 = true;
				}
				
				++c;
			}
			
			isCorrectSubs = bToken2;
		}
		else
		{
			if(curcost < prevbestcost)
			{
				prevbestcost = curcost;
				
				if(bestprev)
					delete [] bestprev;
				
				bestprev = coordinate;
				
				bool bToken1 = false;
				bool bToken2 = false;
				size_t c=0;
				
				while( (c != GetDimension()) && !bToken2)
				{
					if(coordcurr[c] != /*prevcurr*/coordinate[c])
					{
						if(!bToken1)
							bToken1 = true;
						else
							bToken2 = true;
					}
					
					++c;
				}
				
				isCorrectSubs = bToken2;
			}
			else if( (m_HypRefStatus == 1) && (curcost == prevbestcost) && !isCorrectSubs)
			{
				bool bToken1 = false;
				bool bToken2 = false;
				size_t c=0;
				
				while( (c != GetDimension()) && !bToken2)
				{
					if(coordcurr[c] != coordinate[c])
					{
						if(!bToken1)
							bToken1 = true;
						else
							bToken2 = true;
					}
					
					++c;
				}
				
				if(bToken2)
				{
					prevbestcost = curcost;
					
					if(bestprev)
						delete [] bestprev;
					
					bestprev = coordinate;
					isCorrectSubs = bToken2;
				}
			}
			else if( (curcost == prevbestcost) && (m_HypRefStatus != 1) )
			{
				size_t numchg = NumberChanged(coordcurr, coordinate);
				
				if(numchg > bestnumchg)
				{
					prevbestcost = curcost;
					
					if(bestprev)
						delete [] bestprev;
					
					bestprev = coordinate;
					bestnumchg = numchg;
				}	
			}
		}
		
		listPrevCoordinates.NextPosition(i);
		
		if(coordinate && (bestprev != coordinate))
			delete [] coordinate;
	}
	
	// Start cleaning memory
	listPrevCoordinates.RemoveAll();

	// End cleaning memory
	
	return bestprev;
}

/** Print the Levenshtein array */
void Graph::PrintLevenshteinArray()
{
	cout << "==" << endl << "Levenshtein array:" << endl;
	cout << m_MapCost->ToString() << endl << "==" << endl;
}

/** Returns the number of coordinates which have changed */
size_t Graph::NumberChanged(size_t* coord1, size_t* coord2)
{
	uint outNum = 0;
	
	for(size_t i=0; i<GetDimension(); ++i)
	{
		if(coord1[i] != coord2[i])
			++outNum;
	}
	
	return outNum;
}

bool Graph::ValidateTransitionInsertionDeletion(size_t* coordcurr, size_t* coordprev)
{
	if(!m_bPruneOptimization && !m_bWordOptimization)
		return true;
	
	Token* pToken;
	size_t nrbchanged = NumberChanged(coordcurr, coordprev);
		
	if( (nrbchanged == 1) && m_bPruneOptimization )// Insertion or Deletion
	{
		size_t chgdim = 0;
		
		while( (chgdim<GetDimension()) && (coordcurr[chgdim] == coordprev[chgdim]) )
			++chgdim;
				
		pToken = m_TabVecHypRef[chgdim][coordcurr[chgdim]];
		
		int currentchgbegin;
		int currentchgend;
		
		if(pToken)
		{
			currentchgbegin = pToken->GetStartTime();
			
			if(currentchgbegin < 0)
			{
				currentchgbegin = pToken->GetParentSegment()->GetStartTime();
				
				if(currentchgbegin < 0)
					return true;
			}
			else
				return true;
				
			currentchgend = pToken->GetEndTime();
			
			if(currentchgend < 0)
			{
				currentchgend = pToken->GetParentSegment()->GetEndTime();
				
				if(currentchgend < 0)
					return true;
			}
			else
				return true;
		}
		else
			return true;

		int currentstaybegin;
		int currentstayend;
		
		for(size_t i=0; i<GetDimension(); ++i)
		{
			if(i != chgdim)
			{
				pToken = m_TabVecHypRef[i][coordcurr[i]];
				
				if(pToken)
				{
					currentstaybegin = pToken->GetStartTime();
					
					if(currentstaybegin < 0)
					{
						currentstaybegin = pToken->GetParentSegment()->GetStartTime();
						
						if(currentstaybegin < 0)
							return true;
					}
					
					currentstayend = pToken->GetEndTime();
					
					if(currentstayend < 0)
					{
						currentstayend = pToken->GetParentSegment()->GetEndTime();
						
						if(currentstayend < 0)
							return true;
					}
				}
				else
					return true;
				
				if(currentchgbegin < currentstaybegin)
				{
					int gap = 0;
			
					if(currentchgend < currentstaybegin)      // Change before Stay
					{
						gap = currentstaybegin - currentchgend;
					}
					else if(currentstayend < currentchgbegin) // Stay before Change
					{
						gap = currentchgbegin - currentstayend;
					}
					
					if(gap > m_PruneOptimizationThreshold)
						return false;
				}
				
				/*
				if(currentchgbegin < currentstaybegin)
				{
					if(fabs(currentchgbegin-currentstaybegin) > m_PruneOptimizationThreshold)
						return false;
				}
				*/
			}
		}
		
		return true;
	}
	else if( (nrbchanged == 2) && m_bWordOptimization )//Subsitution or Correct
	{
		size_t coord1 = 0;
		size_t coord2 = 0;
		bool bcoord1 = false;
			
		for(size_t i=0; i<GetDimension(); ++i)
		{
			if(coordcurr[i] != coordprev[i])
			{
				if(!bcoord1)
				{
					bcoord1 = true;
					coord1 = i;
				}
				else
					coord2 = i;
			}
		}
		
		int start1 = m_TabVecHypRef[coord1][coordcurr[coord1]]->GetStartTime();
		
		if(start1 < 0)
			start1 = m_TabVecHypRef[coord1][coordcurr[coord1]]->GetParentSegment()->GetStartTime();
		
		int start2 = m_TabVecHypRef[coord2][coordcurr[coord2]]->GetStartTime();
		
		if(start2 < 0)
			start2 = m_TabVecHypRef[coord2][coordcurr[coord2]]->GetParentSegment()->GetStartTime();
		
		int end1 = m_TabVecHypRef[coord1][coordcurr[coord1]]->GetEndTime();
		
		if(end1 < 0)
			end1 = m_TabVecHypRef[coord1][coordcurr[coord1]]->GetParentSegment()->GetEndTime();
		
		int end2 = m_TabVecHypRef[coord2][coordcurr[coord2]]->GetEndTime();
		
		if(end2 < 0)
			end2 = m_TabVecHypRef[coord2][coordcurr[coord2]]->GetParentSegment()->GetEndTime();
			
		if( (start1 < 0) || (start2 < 0) || (end1 < 0) || (end2 < 0) )
			return true;
		
		int gap;
		
		if(end1 < start2)      // 1 before 2
		{
			gap = start2 - end1;
		}
		else if(end2 < start1) // 2 before 1
		{
			gap = start1 - end2;
		}
		else
		{
			gap = 0;
		}
		
		return(gap <= m_WordOptimizationThreshold);
	}
	
	return true;
}

int Graph::GetCostAdaptive(Token* pToken1, Token* pToken2)
{
	if(!m_bAdaptiveCostOptimization)
		return 0;
		
	int start1 = pToken1->GetStartTime();
		
	if(start1 < 0)
		start1 = pToken1->GetParentSegment()->GetStartTime();
	
	int start2 = pToken2->GetStartTime();
	
	if(start2 < 0)
		start2 = pToken2->GetParentSegment()->GetStartTime();
	
	int end1 = pToken1->GetEndTime();
	
	if(end1 < 0)
		end1 = pToken1->GetParentSegment()->GetEndTime();
	
	int end2 = pToken2->GetEndTime();
	
	if(end2 < 0)
		end2 = pToken2->GetParentSegment()->GetEndTime();
		
	if( (start1 < 0) || (start2 < 0) || (end1 < 0) || (end2 < 0) )
		return 0;
	
	int gap;
	
	if(end1 < start2)      // 1 before 2
		gap = start2 - end1;
	else if(end2 < start1) // 2 before 1
		gap = start1 - end2;
	else
		return 0;
		
	return( (int)( m_CostAdaptive*gap/m_MaxDurationSegmentGroup ) );
}

void Graph::SetGraphOptimization()
{
	m_bPruneOptimization = (string("true").compare(Properties::GetProperty("align.timepruneoptimization")) == 0);
	m_bWordOptimization = (string("true").compare(Properties::GetProperty("align.timewordoptimization")) == 0);
	m_bSpeakerOptimization = (string("true").compare(Properties::GetProperty("align.speakeroptimization")) == 0);
	m_bAdaptiveCostOptimization = (string("true").compare(Properties::GetProperty("align.adaptivecost")) == 0);
	
	if(m_bPruneOptimization)
		m_PruneOptimizationThreshold = atoi(Properties::GetProperty("align.timepruneoptimizationthreshold").c_str());
	
	if(m_bWordOptimization)
		m_WordOptimizationThreshold = atoi(Properties::GetProperty("align.timewordoptimizationthreshold").c_str());
}
