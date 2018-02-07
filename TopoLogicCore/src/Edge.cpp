#include <Edge.h>
#include <GlobalCluster.h>
#include <Vertex.h>
#include <Wire.h>

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRep_Tool.hxx>
#include <Geom_BSplineCurve.hxx>
#include <Geom_Point.hxx>
#include <GeomAPI_Interpolate.hxx>
#include <gp_Lin.hxx>
#include <Precision.hxx>
#include <ShapeAnalysis_Edge.hxx>
#include <TopoDS.hxx>
#include <TopExp.hxx>

#include <assert.h>

namespace TopoLogicCore
{
	Vertex* Edge::StartVertex() const
	{
		ShapeAnalysis_Edge shapeAnalysisEdge;
		return new Vertex(new TopoDS_Vertex(shapeAnalysisEdge.FirstVertex(TopoDS::Edge(*GetOcctShape()))));
	}

	Vertex* Edge::EndVertex() const
	{
		ShapeAnalysis_Edge shapeAnalysisEdge;
		return new Vertex(new TopoDS_Vertex(shapeAnalysisEdge.LastVertex(TopoDS::Edge(*GetOcctShape()))));
	}

	void Edge::Wires(std::list<Wire*>& rWires) const
	{
		TopTools_IndexedDataMapOfShapeListOfShape edgeToWireMap;
		TopExp::MapShapesAndUniqueAncestors(*GlobalCluster::GetInstance().GetCluster()->GetOcctShape(), TopAbs_EDGE, TopAbs_WIRE, edgeToWireMap);

		const TopTools_ListOfShape& rkOCCTWires = edgeToWireMap.FindFromKey(*m_pOcctEdge);

		for (TopTools_ListOfShape::const_iterator kIterator = rkOCCTWires.cbegin();
			kIterator != rkOCCTWires.cend();
			kIterator++)
		{
			if (kIterator->ShapeType() == TopAbs_WIRE)
			{
				const TopoDS_Wire& rkOcctWire = TopoDS::Wire(*kIterator);
				rWires.push_back(new Wire(new TopoDS_Wire(rkOcctWire)));
			}
		}
	}

	Edge* Edge::ByCurve(
		const TColgp_Array1OfPnt &rkOcctPoles,
		const TColStd_Array1OfReal &rkOcctWeights,
		const TColStd_Array1OfReal &rkOcctKnots,
		const TColStd_Array1OfInteger &rkOcctMultiplicities,
		const Standard_Integer kDegree,
		const Standard_Boolean kIsPeriodic,
		const Standard_Boolean kIsRational)
	{
		BRepBuilderAPI_MakeEdge occtMakeEdge;
		Handle(Geom_BSplineCurve) pOcctBSplineCurve = new Geom_BSplineCurve(
			rkOcctPoles,
			rkOcctWeights,
			rkOcctKnots,
			rkOcctMultiplicities,
			kDegree,
			false,
			kIsRational);

		// Done separately, because SetPeriod() does additional steps to adjust the poles, weights, etc.
		if (kIsPeriodic)
		{
			pOcctBSplineCurve->SetPeriodic();
		}

		return ByCurve(pOcctBSplineCurve);
	}

	Edge* Edge::ByCurve(Handle(Geom_Curve) pOcctCurve, const double rkParameter1, const double rkParameter2)
	{
		const double kOcctFirstParameter = pOcctCurve->FirstParameter();
		const double kOcctLastParameter = pOcctCurve->LastParameter();
		const double kOcctDeltaParameter = kOcctLastParameter - kOcctFirstParameter;

		// Compute the non-normalised parameters.
		const double kOcctParameter1 = kOcctFirstParameter + rkParameter1 * kOcctDeltaParameter;
		const double kOcctParameter2 = kOcctFirstParameter + rkParameter2 * kOcctDeltaParameter;

		return new Edge(new TopoDS_Edge(BRepBuilderAPI_MakeEdge(pOcctCurve, kOcctParameter1, kOcctParameter2)));
	}

	Edge* Edge::ByVertices(const std::list<Vertex*>& rkVertices)
	{
		// Cases:
		// - 0 or 1 vertex --> invalid
		// - 2 --> line
		// - 3 --> BSpline

		int numberOfVertices = (int) rkVertices.size();
		if (numberOfVertices < 2)
		{
			throw std::exception("Too few vertices to create an edge.");
		}
		else if (numberOfVertices == 2) // a line
		{
			Vertex* pVertex1 = *(rkVertices.begin());
			Vertex* pVertex2 = *(rkVertices.begin()++);
			return new Edge(new TopoDS_Edge(BRepBuilderAPI_MakeEdge(TopoDS::Vertex(*pVertex1->GetOcctShape()), 
																	TopoDS::Vertex(*pVertex2->GetOcctShape()))));
		}

		// else more than 2 vertices
		Handle(TColgp_HArray1OfPnt) pOcctPoints = new TColgp_HArray1OfPnt(1, numberOfVertices);
		int i = 1;
		for(std::list<Vertex*>::const_iterator kVertexIterator = rkVertices.begin();
			kVertexIterator != rkVertices.end();
			kVertexIterator++)
		{
			Vertex* pVertex = *kVertexIterator;
			pOcctPoints->SetValue(i, pVertex->Point()->Pnt());
			++i;
		}
		GeomAPI_Interpolate occtInterpolate(pOcctPoints, false, Precision::Confusion());
		occtInterpolate.Perform();
		Handle(Geom_Curve) pOcctCurveOnTargetSurface = occtInterpolate.Curve();

		return new Edge(new TopoDS_Edge(BRepBuilderAPI_MakeEdge(pOcctCurveOnTargetSurface)));
	}

	Vertex* Edge::SharedVertex(Edge const * const kpkAnotherEdge) const
	{
		TopoDS_Vertex* pOcctSharedVertex = new TopoDS_Vertex();
		bool result = TopExp::CommonVertex(*m_pOcctEdge, TopoDS::Edge(*kpkAnotherEdge->GetOcctShape()), *pOcctSharedVertex);
		if (!result)
		{
			delete pOcctSharedVertex;
			return nullptr;
		}

		return new Vertex(pOcctSharedVertex);
	}

	void Edge::Geometry(std::list<Handle(Geom_Geometry)>& rOcctGeometries) const
	{
		rOcctGeometries.push_back(Curve());
	}

	TopoDS_Shape* Edge::GetOcctShape() const
	{
		assert(m_pOcctEdge != nullptr && "Edge::m_pOcctEdge is null.");
		if (m_pOcctEdge == nullptr)
		{
			throw std::exception("Edge::m_pOcctEdge is null.");
		}

		return m_pOcctEdge;
	}

	Handle(Geom_Curve) Edge::Curve() const
	{
		// TODO: do these parameters need to be stored?
		double u0 = 0.0, u1 = 0.0;
		return BRep_Tool::Curve(TopoDS::Edge(*GetOcctShape()), u0, u1);
	}

	Edge::Edge(TopoDS_Edge * const kpOcctEdge)
		: Topology(1)
		, m_pOcctEdge(kpOcctEdge)
	{
		GlobalCluster::GetInstance().Add(this);
	}

	Edge::~Edge()
	{
		GlobalCluster::GetInstance().Remove(this);
		delete m_pOcctEdge;
	}
}
