
/*
 * Copyright (C) 2000
 * Ferdinando Ametrano, Luigi Ballabio, Adolfo Benin, Marco Marchioro
 * 
 * This file is part of QuantLib.
 * QuantLib is a C++ open source library for financial quantitative
 * analysts and developers --- http://quantlib.sourceforge.net/
 *
 * QuantLib is free software and you are allowed to use, copy, modify, merge,
 * publish, distribute, and/or sell copies of it under the conditions stated 
 * in the QuantLib License.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
 *
 * You should have received a copy of the license along with this file;
 * if not, contact ferdinando@ametrano.net
 *
 * QuantLib license is also available at http://quantlib.sourceforge.net/LICENSE.TXT
*/

#include "pricers.h"

namespace QuantLib {
	namespace Pricers {
	using FiniteDifferences::FiniteDifferenceModel;
	using FiniteDifferences::CrankNicolson;
	using FiniteDifferences::StepCondition;
	using FiniteDifferences::TridiagonalOperator;

		DividendAmericanOption::DividendAmericanOption(Option::Type type, double underlying, double strike,
		  Rate underlyingGrowthRate, Rate riskFreeRate,	Time residualTime, double volatility,
		   std::vector<double> dividends, std::vector<Time> exdivdates)
			:theDividends(dividends),theExDivDates(exdivdates), theNumberOfDivs(dividends.size()),
			BSMNumericalOption(type, underlying-addElements(dividends), strike,underlyingGrowthRate,riskFreeRate,residualTime,volatility,
			  (residualTime>1.0 ? (int)(100+(residualTime-1.0)*50) : 100)*4+1){

			Require(theNumberOfDivs==theExDivDates.size(),"the number of dividends is diffrent from	that of	dates");
			Require(theNumberOfDivs>=1,"the	number of dividends must be at least one");
			Require(underlying-addElements(dividends)>0,"Dividends cannot exceed underlying");
			Require(theExDivDates[0]>0,"The	ex dividend dates must be positive");
			Require(theExDivDates[theExDivDates.size()-1]<residualTime,"The	ex dividend dates must be within the residual time");
			Require(theDividends[0]>=0,"Dividends cannot be	negative");
			for(unsigned int j=1; j<theNumberOfDivs;j++){
				Require(theExDivDates[j-1]<theExDivDates[j],"Dividend dates must be in increasing order");
				Require(theDividends[j]>=0,"DividendAmericanOption: Dividends cannot be	negative");
			}
		}

		double DividendAmericanOption::value() const {

			if (!hasBeenCalculated)	{
			
			  theOptionIsAmerican = true;
				setGridLimits();
				initializeGrid();
				initializeInitialCondition();
				initializeOperator();
				Array prices = theInitialPrices;
				Array controlPrices = theInitialPrices;

				Time beginDate = theResidualTime;
				int j=theNumberOfDivs-1;
				do{
			    Handle<StepCondition<Array>	> americanCondition(new	BSMAmericanCondition(theInitialPrices));
					FiniteDifferenceModel<CrankNicolson<TridiagonalOperator> > model(theOperator);
					Time endDate;
					if(j >=	0)
						endDate	= theExDivDates[j];
					else
						endDate	= 0.0;
					if(theOptionIsAmerican)
						model.rollback(prices,beginDate,endDate,200,americanCondition);
					else
						model.rollback(prices,beginDate,endDate,200);
					model.rollback(controlPrices,beginDate,endDate,200);
					beginDate = endDate;

					if(j >=	0){
						Array oldGrid =	theGrid;

						double centre =	valueAtCenter(theGrid);
						double mltp = centre/theGrid[0];
						double newMltp = mltp/(1+(mltp-1)*theDividends[j]/(centre+theDividends[j]));
						Require(newMltp>1,"Dividends are to big");
						sMin =	(centre+theDividends[j])/newMltp;
						sMax =	(centre+theDividends[j])*newMltp;
						initializeGrid();
						initializeInitialCondition();
						initializeOperator();
						movePricesBeforeExDiv(theDividends[j], theGrid,	 prices, 				oldGrid);
						movePricesBeforeExDiv(theDividends[j], theGrid,	 controlPrices, oldGrid);
					}
			}while(--j>=-1);
			  DividendEuropeanOption analitic(theType,theUnderlying+addElements(theDividends),
			  	theStrike,theUnderlyingGrowthRate,theRiskFreeRate,theResidualTime,theVolatility,
			  	theDividends,theExDivDates);
				theValue = valueAtCenter(prices)-valueAtCenter(controlPrices)+analitic.value();
				theDelta = firstDerivativeAtCenter(prices,theGrid)-firstDerivativeAtCenter(controlPrices,theGrid)+analitic.delta();
				theGamma = secondDerivativeAtCenter(prices,theGrid)-secondDerivativeAtCenter(controlPrices,theGrid)+analitic.gamma();
				theTheta = 0.0;	//!To be fixed,	eventually
				hasBeenCalculated = true;
			}
			return theValue;
		}

		void DividendAmericanOption::movePricesBeforeExDiv(double Div, 
			const Array& newGrid, Array& prices, const Array& oldGrid) const{

			int j;
			Array vOldGrid(oldGrid+Div);
			NewCubicSpline priceSpline(vOldGrid, prices);
			if(theOptionIsAmerican){
				for(j=0;j<prices.size();j++);
					prices[j] = QL_MAX(priceSpline.value(newGrid[j], j),theInitialPrices[j]);
			}else{
				for(j=0;j<prices.size();j++);
					prices[j] = priceSpline.value(newGrid[j], j);
			}
		}
	}
}
