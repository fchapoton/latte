#include <iostream>
using namespace std;

// #define DEBUG_IRRATIONAL

#include <cassert>

#include "Irrational.h"
#include "dual.h"
#include "convert.h"
#include "print.h"
#include "vertices/cdd.h"
#include "ramon.h"

static rationalVector *
computeConeStabilityCube_simplicial(listCone *cone, int numOfVars, 
				    ZZ &length_numerator,
				    ZZ &length_denominator)
{
  ZZ vertex_denominator;
  vec_ZZ vertex_numerator
    = scaleRationalVectorToInteger(cone->vertex, numOfVars,
				   vertex_denominator);
#ifdef DEBUG_IRRATIONAL
  cout << "vertex = " << vertex_numerator << " / "
       << vertex_denominator << endl;
#endif
  // Compute vertex multipliers, multiplied by the determinant
  mat_ZZ dual = createFacetMatrix2(cone, numOfVars, numOfVars);
#ifdef DEBUG_IRRATIONAL
  cout << "dual (-B^{-1}) = " << dual << endl;
#endif
  vec_ZZ scaled_D_lambda = dual * vertex_numerator;
  // Round down to go to the bottom of the known stability region
  int i;
  for (i = 0; i<numOfVars; i++) {
    ZZ l = scaled_D_lambda[i];
    div(scaled_D_lambda[i], l, vertex_denominator);
  }

#ifdef DEBUG_IRRATIONAL
  cout << "bottom multipliers: " << scaled_D_lambda << endl;
#endif
  // Now move to the center; read it as the numerator over 2*D
  ZZ D = abs(cone->determinant);
  for (i = 0; i<numOfVars; i++)
    scaled_D_lambda[i] = scaled_D_lambda[i] * 2 + 1;
  vec_ZZ center_numerator
    =  (-transpose(createConeDecMatrix(cone, numOfVars, numOfVars))
	* scaled_D_lambda);
  ZZ center_denominator = 2 * D;
#ifdef DEBUG_IRRATIONAL
  cout << "--center--> " << center_numerator << " / "
       << center_denominator << endl; 
#endif
  // Compute stability length
  length_numerator = 1;
  ZZ dual_1_norm, max_dual_1_norm;
  max_dual_1_norm = 0;
  for (i = 0; i<numOfVars; i++) {
    int j;
    dual_1_norm = 0;
    for (j = 0; j<numOfVars; j++)
      dual_1_norm += abs(dual[i][j]);
    if (dual_1_norm > max_dual_1_norm)
      max_dual_1_norm = dual_1_norm;
  }
  length_denominator = 2 * max_dual_1_norm;
  // Check whether at least the new vertex is OK.
  // (We do not check the length.)
  rationalVector *v = createRationalVector(numOfVars);
  for (i = 0; i<numOfVars; i++) {
    v->set_entry(i, center_numerator[i], center_denominator);
  }
  assertConesIntegerEquivalent(cone, v, numOfVars,
			       "Not integer equivalent with center");
  
  return v;
}

static rationalVector *
computeConeStabilityCube_general(listCone *cone, int numOfVars, 
				 ZZ &length_numerator,
				 ZZ &length_denominator)
{
  ZZ vertex_scale_factor;
  vec_ZZ scaled_vertex
    = scaleRationalVectorToInteger(cone->vertex, numOfVars,
				   vertex_scale_factor);
  listVector *matrix = NULL;
  listVector *facet;
  int i;
  for (facet = cone->facets; facet; facet = facet->rest) {
      ZZ sp;
      InnerProduct(sp, facet->first, scaled_vertex);
      ZZ floor;
      div(floor, sp, vertex_scale_factor);
      ZZ abs_sum;
      for (i = 0; i<numOfVars; i++)
	abs_sum += abs(facet->first[i]);
      vec_ZZ ineq;
      ineq.SetLength(numOfVars + 2);
      // First inequality:
      //   <f,x> + ||f||_1 lambda <= floor(<f,v>) + 1.
      ineq[0] = floor + 1;
      for (i = 0; i<numOfVars; i++)
	ineq[i+1] = -facet->first[i];
      ineq[numOfVars + 1] = -abs_sum;
      matrix = new listVector(ineq, matrix);
      // Second inequality:
      //  -<f,x> + ||f||_1 lambda <= -floor(<f,v>).
      ineq[0] = -floor;
      for (i = 0; i<numOfVars; i++)
	ineq[i+1] = facet->first[i];
      ineq[numOfVars + 1] = -abs_sum;
      matrix = new listVector(ineq, matrix);
  }
  vec_ZZ cost;
  cost.SetLength(numOfVars + 1);
  cost[numOfVars] = 1;
  rationalVector *optimal_solution
    = LP(matrix, cost, numOfVars + 1, false); //maximizes
  freeListVector(matrix);
  length_numerator = optimal_solution->numerators()[numOfVars];
  length_denominator = optimal_solution->denominators()[numOfVars];
  rationalVector *center = createRationalVector(numOfVars);
  for (i = 0; i<numOfVars; i++) {
    center->set_entry(i, optimal_solution->numerators()[i],
		      optimal_solution->denominators()[i]);
  }
  delete optimal_solution;
  return center;
}

rationalVector *
computeConeStabilityCube(listCone *cone, int numOfVars, 
			 ZZ &length_numerator, ZZ &length_denominator)
{
  if (cone->facets == NULL)
    computeDetAndFacetsOfSimplicialCone(cone, numOfVars);
  bool our_simplicial_facets
    = (cone->facet_divisors.length() == numOfVars);
  if (our_simplicial_facets) {
    // Here we use the order and the properties of the facet vectors
    // that we have computed:
    // < RAY_i, FACET_j > = -FACET_DIVISOR_i * DELTA_{i,j}.
    return computeConeStabilityCube_simplicial(cone, numOfVars,
					       length_numerator,
					       length_denominator);
  }
  else {
    assert(cone->facet_divisors.length() == 0);
    // In this case, there is no order of the facet vectors,
    // even if the cone happens to be simplicial.
    return computeConeStabilityCube_general(cone, numOfVars,
					    length_numerator,
					    length_denominator);
  }
}

static ZZ
lcm(const ZZ& a, const ZZ& b)
{
  return a * ( b / GCD(a, b));
}

int
estimate_barvinok_depth(listCone *cone, int numOfVars)
{
  ZZ det_estimate;
  if (IsZero(cone->determinant)) {
    ZZ max_norm_square;
    listVector *ray;
    for (ray = cone->rays; ray; ray = ray->rest) {
      ZZ norm_square;
      int j;
      for (j = 0; j<numOfVars; j++)
	norm_square += ray->first[j] * ray->first[j];
      if (norm_square > max_norm_square)
	max_norm_square = norm_square;
    }
    det_estimate = power(max_norm_square, (numOfVars + 1) / 2);
  }
  else
    det_estimate = abs(cone->determinant);

  return 1 + (int) ceil(log((double) NumBits(det_estimate))
			/ log(1 + 1.0 / (numOfVars - 1)));
}

void
irrationalizeCone(listCone *cone, int numOfVars)
{
  ZZ center_denominator;
  vec_ZZ center_numerator;
  ZZ stability_length_numerator, stability_length_denominator;
  {
    rationalVector *stability_center
      = computeConeStabilityCube(cone, numOfVars,
				 stability_length_numerator,
				 stability_length_denominator);
    center_numerator =
      scaleRationalVectorToInteger(stability_center, numOfVars,
				   center_denominator);
#ifdef DEBUG_IRRATIONAL
    cout << "stability center: ";
    printRationalVector(stability_center, numOfVars);
    cout << "stability length: " << stability_length_numerator
	 << "/" << stability_length_denominator << endl;
#endif
    delete stability_center;
  }
  // The actual irrationalization.
  ZZ M;
  // Value from math.CO/0603308 v1 (wrong):
  //M = 2 * power(D, numOfVars + 1);
  // Value from v2:
  {
    int barvinok_depth
      = estimate_barvinok_depth(cone, numOfVars);
#ifdef DEBUG_IRRATIONAL
    cout << "Estimated tree depth " << barvinok_depth << endl;
#endif
    ZZ C;
    C = 0;
    int i;
    listVector *ray;
    for (ray = cone->rays; ray; ray = ray->rest) {
      for (i = 0; i<numOfVars; i++) {
	if (abs(ray->first[i]) > C)
	  C = abs(ray->first[i]);
      }
    }
    ZZ factorial;
    factorial = 1;
    for (i = 2; i<=numOfVars - 1; i++)
      factorial *= i;
    ZZ d;
    d = numOfVars;
    M = 2 * power(power(d, barvinok_depth) * C,
		  numOfVars - 1);
  }
  // Now use this value of M to compute the irrationalization.
  vec_ZZ irrationalizer_numerator;
  irrationalizer_numerator.SetLength(numOfVars);
  ZZ irrationalizer_denominator;
  ZZ TwoM_power;
  TwoM_power = 1;
  int i;
  for (i = numOfVars-1; i>=0; i--) {
    irrationalizer_numerator[i] = TwoM_power * stability_length_numerator;
    TwoM_power *= (2 * M);
  }
  irrationalizer_denominator = 2 * (TwoM_power / M) * stability_length_denominator;

  ZZ common_denominator = lcm(center_denominator, irrationalizer_denominator);
  // Store the new vertex
  rationalVector *new_vertex = createRationalVector(numOfVars);
  for (i = 0; i<numOfVars; i++) {
    new_vertex->set_entry(i, 
			  center_numerator[i] * (common_denominator / center_denominator)
			  + irrationalizer_numerator[i] * (common_denominator / irrationalizer_denominator),
			  common_denominator);
  }
#ifdef DEBUG_IRRATIONAL
  cout << "--irrationalize--> ";
  printRationalVector(new_vertex, numOfVars);
#endif
  canonicalizeRationalVector(new_vertex, numOfVars);
#ifdef DEBUG_IRRATIONAL
  cout << "--canonicalize---> ";
  printRationalVector(new_vertex, numOfVars);
#endif
  assertConesIntegerEquivalent(cone, cone->vertex, numOfVars,
			       "cone and cone not integer equivalent");
  assertConesIntegerEquivalent(cone, new_vertex, numOfVars,
			       "Not integer equivalent with new_vertex");
  delete cone->vertex;
  cone->vertex = new_vertex;
  assert(isConeIrrational(cone, numOfVars));
}

void
irrationalizeCones(listCone *cones, int numOfVars)
{
  listCone *cone;
  for (cone = cones; cone != NULL; cone=cone->rest)
    irrationalizeCone(cone, numOfVars);
}

bool
isConeIrrational(listCone *cone, int numOfVars)
{
  /* The affine hulls of the proper faces do not contain any integer
     points if and only if the scalar product of the integrally
     scaled, primitive dual basis vectors with the rational vertex is
     non-integral. */
  ZZ vertex_denominator;
  vec_ZZ vertex_numerator
    = scaleRationalVectorToInteger(cone->vertex, numOfVars,
				   vertex_denominator);
  ZZ scaled_sp;
  listVector *facet;
  for (facet = cone->facets; facet; facet = facet->rest) {
    InnerProduct(scaled_sp, vertex_numerator, facet->first);
    if (divide(scaled_sp, vertex_denominator))
      return false;
  }
  return true;
}

void
checkConeIrrational(listCone *cone, int numOfVars)
{
#if 1
  if (not isConeIrrational(cone, numOfVars)) {
    static NotIrrationalException notirrational;
    throw notirrational;
  }
#endif
}

void
assertConesIntegerEquivalent(listCone *cone1, rationalVector *new_vertex,
			     int numOfVars, const char *message)
{
  ZZ vertex1_denominator;
  vec_ZZ vertex1_numerator
    = scaleRationalVectorToInteger(cone1->vertex, numOfVars,
				   vertex1_denominator);
  ZZ vertex2_denominator;
  vec_ZZ vertex2_numerator
    = scaleRationalVectorToInteger(new_vertex, numOfVars,
				   vertex2_denominator);
  ZZ scaled_sp_1, scaled_sp_2;
  ZZ interval_1, interval_2;
  listVector *facet1;
  for (facet1 = cone1->facets; facet1; facet1 = facet1->rest) {
    InnerProduct(scaled_sp_1, vertex1_numerator, facet1->first);
    InnerProduct(scaled_sp_2, vertex2_numerator, facet1->first);
    div(interval_1, scaled_sp_1, vertex1_denominator);
    div(interval_2, scaled_sp_2, vertex2_denominator);
    if (interval_1 != interval_2) {
      cerr << message << endl;
      assert(interval_1 == interval_2);
    }
  }
}

